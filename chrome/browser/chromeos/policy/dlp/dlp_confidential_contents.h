// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_

#include <list>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace policy {

// Keeps track of title and corresponding icon of a WebContents object.
// Used to cache and later show information about observed confidential contents
// to the user.
struct DlpConfidentialContent {
  DlpConfidentialContent() = delete;
  // Constructs DlpConfidentialContent from the title and icon obtained from
  // |web_contents|, which cannot be null.
  explicit DlpConfidentialContent(content::WebContents* web_contents);
  // Constructs DlpConfidentialContent from the title and icon obtained from
  // |window|, which cannot be null and |url|.
  DlpConfidentialContent(aura::Window* window, const GURL& url);
  DlpConfidentialContent(const DlpConfidentialContent& other);
  DlpConfidentialContent& operator=(const DlpConfidentialContent& other);
  ~DlpConfidentialContent() = default;

  // Contents with the same url are considered equal, ignoring the ref (part
  // after #).
  bool operator==(const DlpConfidentialContent& other) const;
  bool operator!=(const DlpConfidentialContent& other) const;
  bool operator<(const DlpConfidentialContent& other) const;
  bool operator<=(const DlpConfidentialContent& other) const;
  bool operator>(const DlpConfidentialContent& other) const;
  bool operator>=(const DlpConfidentialContent& other) const;

  gfx::ImageSkia icon;
  std::u16string title;
  GURL url;
};

// Provides basic functions for storing and working with DLP confidential
// contents.
class DlpConfidentialContents {
 public:
  DlpConfidentialContents();
  explicit DlpConfidentialContents(
      const std::vector<content::WebContents*>& web_contents);
  DlpConfidentialContents(const DlpConfidentialContents& other);
  DlpConfidentialContents& operator=(const DlpConfidentialContents& other);
  ~DlpConfidentialContents();

  friend bool operator==(const DlpConfidentialContents& a,
                         const DlpConfidentialContents& b) {
    return a.contents_ == b.contents_;
  }
  friend bool operator!=(const DlpConfidentialContents& a,
                         const DlpConfidentialContents& b) {
    return !(a == b);
  }

  // Returns true if all the elements in |a| and |b| are equal (i.e. have the
  // same url) and the same title, and false otherwise. Useful to detect changes
  // in titles, even if the set of the confidential contents hasn't changed.
  friend bool EqualWithTitles(const DlpConfidentialContents& a,
                              const DlpConfidentialContents& b) {
    return base::ranges::equal(
        a.contents_, b.contents_,
        [](const DlpConfidentialContent& x, const DlpConfidentialContent& y) {
          return x == y && x.title == y.title;
        });
  }

  // Returns a reference to the underlying content container.
  base::flat_set<DlpConfidentialContent>& GetContents();

  // Returns a const reference to the underlying content container.
  const base::flat_set<DlpConfidentialContent>& GetContents() const;

  // Converts |web_contents| to a DlpConfidentialContent and adds it to the
  // underlying container.
  void Add(content::WebContents* web_contents);
  // Same for |window| and |url| pair.
  void Add(aura::Window* window, const GURL& url);

  // Removes all stored confidential content, if there was any, and adds
  // |web_contents| converted to a DlpConfidentialContent.
  void ClearAndAdd(content::WebContents* web_contents);
  // Same for |window| and |url| pair.
  void ClearAndAdd(aura::Window* web_contents, const GURL& url);

  // Returns whether there is any content stored or not.
  bool IsEmpty() const;

  // Adds all content stored in |other| to the underlying container, without
  // duplicates.
  void InsertOrUpdate(const DlpConfidentialContents& other);

 private:
  base::flat_set<DlpConfidentialContent> contents_;
};

// Used to avoid warning the user for an action and content that they already
// acknowledged and bypassed a warning for, by caching these contents for a
// certain amount of time.
//
// Automatically evicts entries after a timeout.
// If the number of cached entries exceeds a predefined limits, evicts the
// oldest entry from the cache.
class DlpConfidentialContentsCache {
 public:
  DlpConfidentialContentsCache();
  DlpConfidentialContentsCache(const DlpConfidentialContentsCache& other) =
      delete;
  DlpConfidentialContentsCache& operator=(
      const DlpConfidentialContentsCache& other) = delete;
  ~DlpConfidentialContentsCache();

  // Creates and stores an entry from |web_contents| and |restriction|.
  void Cache(const DlpConfidentialContent& content,
             DlpRulesManager::Restriction restriction);

  // Returns true if there is a cached entry corresponding to |web_contents| and
  // |restriction|.
  // Useful to avoid converting |web_contents| to a DlpConfidentialContent
  // unnecessarily.
  bool Contains(content::WebContents* web_contents,
                DlpRulesManager::Restriction restriction) const;

  // Returns true if there is a cached entry corresponding to |content| and
  // |restriction|.
  bool Contains(const DlpConfidentialContent& content,
                DlpRulesManager::Restriction restriction) const;

  // Returns the number of cached entries, useful for testing.
  size_t GetSizeForTesting() const;

  // Returns the duration for which the entries are kept in the cache.
  static base::TimeDelta GetCacheTimeout();

  // Used only in tests to inject a task runner for time control.
  void SetTaskRunnerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  struct Entry {
    Entry() = delete;
    Entry(const DlpConfidentialContent& content,
          DlpRulesManager::Restriction restriction,
          base::TimeTicks timestamp);
    Entry(const Entry& other) = delete;
    Entry& operator=(const Entry& other) = delete;
    ~Entry();

    DlpConfidentialContent content;
    DlpRulesManager::Restriction restriction;
    base::TimeTicks created_at;
    base::OneShotTimer eviction_timer;
  };

  // Starts the |entry|'s eviction timer.
  void StartEvictionTimer(Entry* entry);

  // Evicts an entry corresponding to |content| if it exists, no-op otherwise.
  void OnEvictionTimerUp(const DlpConfidentialContent& content);

  std::list<std::unique_ptr<Entry>> entries_;
  const size_t cache_size_limit_;

  // Used to evict cache entries after the timeout.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_
