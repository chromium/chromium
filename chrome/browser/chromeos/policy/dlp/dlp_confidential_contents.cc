// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"

#include <memory>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace policy {

namespace {

gfx::ImageSkia GetWindowIcon(aura::Window* window) {
  gfx::ImageSkia* image = window->GetProperty(aura::client::kWindowIconKey);
  if (!image)
    image = window->GetProperty(aura::client::kAppIconKey);
  return image ? *image : gfx::ImageSkia();
}

}  // namespace

// The maximum number of entries that can be kept in the
// DlpConfidentialContentsCache.
static constexpr size_t kDefaultCacheSizeLimit = 100;

// The default timeout after which the entries are evicted from the
// DlpConfidentialContentsCache.
static constexpr base::TimeDelta kDefaultCacheTimeout = base::Days(7);

DlpConfidentialContent::DlpConfidentialContent(
    content::WebContents* web_contents)
    : icon(favicon::TabFaviconFromWebContents(web_contents).AsImageSkia()),
      title(web_contents->GetTitle()),
      url(web_contents->GetLastCommittedURL().GetWithoutRef()) {}

DlpConfidentialContent::DlpConfidentialContent(aura::Window* window,
                                               const GURL& url)
    : icon(GetWindowIcon(window)),
      title(window->GetTitle()),
      url(url.GetWithoutRef()) {}

DlpConfidentialContent::DlpConfidentialContent(
    const DlpConfidentialContent& other) = default;
DlpConfidentialContent& DlpConfidentialContent::operator=(
    const DlpConfidentialContent& other) = default;

bool DlpConfidentialContent::operator==(
    const DlpConfidentialContent& other) const {
  return url == other.url;
}

bool DlpConfidentialContent::operator!=(
    const DlpConfidentialContent& other) const {
  return !(*this == other);
}

bool DlpConfidentialContent::operator<(
    const DlpConfidentialContent& other) const {
  return url < other.url;
}

bool DlpConfidentialContent::operator<=(
    const DlpConfidentialContent& other) const {
  return *this == other || *this < other;
}

bool DlpConfidentialContent::operator>(
    const DlpConfidentialContent& other) const {
  return !(*this <= other);
}

bool DlpConfidentialContent::operator>=(
    const DlpConfidentialContent& other) const {
  return !(*this < other);
}

DlpConfidentialContents::DlpConfidentialContents() = default;

DlpConfidentialContents::DlpConfidentialContents(
    const std::vector<content::WebContents*>& web_contents) {
  for (auto* content : web_contents)
    Add(content);
}

DlpConfidentialContents::DlpConfidentialContents(
    const DlpConfidentialContents& other) = default;

DlpConfidentialContents& DlpConfidentialContents::operator=(
    const DlpConfidentialContents& other) = default;

DlpConfidentialContents::~DlpConfidentialContents() = default;

const base::flat_set<DlpConfidentialContent>&
DlpConfidentialContents::GetContents() const {
  return contents_;
}

base::flat_set<DlpConfidentialContent>& DlpConfidentialContents::GetContents() {
  return contents_;
}

void DlpConfidentialContents::Add(content::WebContents* web_contents) {
  contents_.insert(DlpConfidentialContent(web_contents));
}

void DlpConfidentialContents::Add(aura::Window* window, const GURL& url) {
  contents_.insert(DlpConfidentialContent(window, url));
}

void DlpConfidentialContents::ClearAndAdd(content::WebContents* web_contents) {
  contents_.clear();
  Add(web_contents);
}

void DlpConfidentialContents::ClearAndAdd(aura::Window* window,
                                          const GURL& url) {
  contents_.clear();
  Add(window, url);
}

bool DlpConfidentialContents::IsEmpty() const {
  return contents_.empty();
}

void DlpConfidentialContents::InsertOrUpdate(
    const DlpConfidentialContents& other) {
  contents_.insert(other.contents_.begin(), other.contents_.end());
  for (auto other_content : other.contents_) {
    auto it = base::ranges::find_if(
        contents_, [&other_content](const DlpConfidentialContent& content) {
          return content == other_content &&
                 content.title != other_content.title;
        });
    if (it != contents_.end()) {
      *it = other_content;
    }
  }
}

DlpConfidentialContentsCache::Entry::Entry(
    const DlpConfidentialContent& content,
    DlpRulesManager::Restriction restriction,
    base::TimeTicks timestamp)
    : content(content), restriction(restriction), created_at(timestamp) {}

DlpConfidentialContentsCache::Entry::~Entry() = default;

DlpConfidentialContentsCache::DlpConfidentialContentsCache()
    : cache_size_limit_(kDefaultCacheSizeLimit),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

DlpConfidentialContentsCache::~DlpConfidentialContentsCache() = default;

void DlpConfidentialContentsCache::Cache(
    const DlpConfidentialContent& content,
    DlpRulesManager::Restriction restriction) {
  if (Contains(content, restriction)) {
    return;
  }

  auto entry =
      std::make_unique<Entry>(content, restriction, base::TimeTicks::Now());
  StartEvictionTimer(entry.get());
  entries_.push_front(std::move(entry));
  if (entries_.size() > cache_size_limit_) {
    entries_.pop_back();
  }
  data_controls::DlpCountHistogram(
      data_controls::dlp::kConfidentialContentsCount, entries_.size(),
      cache_size_limit_);
}

bool DlpConfidentialContentsCache::Contains(
    content::WebContents* web_contents,
    DlpRulesManager::Restriction restriction) const {
  const GURL url = web_contents->GetLastCommittedURL();
  return base::ranges::any_of(
      entries_, [&](const std::unique_ptr<Entry>& entry) {
        return entry->restriction == restriction &&
               entry->content.url.EqualsIgnoringRef(url);
      });
}

bool DlpConfidentialContentsCache::Contains(
    const DlpConfidentialContent& content,
    DlpRulesManager::Restriction restriction) const {
  return base::ranges::any_of(
      entries_, [&](const std::unique_ptr<Entry>& entry) {
        return entry->restriction == restriction &&
               entry->content.url.EqualsIgnoringRef(content.url);
      });
}

size_t DlpConfidentialContentsCache::GetSizeForTesting() const {
  return entries_.size();
}

// static
base::TimeDelta DlpConfidentialContentsCache::GetCacheTimeout() {
  return kDefaultCacheTimeout;
}

void DlpConfidentialContentsCache::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

void DlpConfidentialContentsCache::StartEvictionTimer(Entry* entry) {
  entry->eviction_timer.SetTaskRunner(task_runner_);
  entry->eviction_timer.Start(
      FROM_HERE, GetCacheTimeout(),
      base::BindOnce(&DlpConfidentialContentsCache::OnEvictionTimerUp,
                     base::Unretained(this), entry->content));
}

void DlpConfidentialContentsCache::OnEvictionTimerUp(
    const DlpConfidentialContent& content) {
  entries_.remove_if([&](const std::unique_ptr<Entry>& entry) {
    return entry.get()->content == content;
  });
}

}  // namespace policy
