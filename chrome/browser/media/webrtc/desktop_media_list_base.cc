// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_list_base.h"

#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

using content::BrowserThread;
using content::DesktopMediaID;

DesktopMediaListBase::DesktopMediaListBase(base::TimeDelta update_period)
    : update_period_(update_period) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
}

DesktopMediaListBase::DesktopMediaListBase(base::TimeDelta update_period,
                                           DesktopMediaListObserver* observer)
    : update_period_(update_period), observer_(observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
}

DesktopMediaListBase::~DesktopMediaListBase() = default;

void DesktopMediaListBase::SetUpdatePeriod(base::TimeDelta period) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!observer_, base::NotFatalUntil::M161);
  update_period_ = period;
}

void DesktopMediaListBase::SetThumbnailSize(const gfx::Size& thumbnail_size) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  thumbnail_size_ = thumbnail_size;
}

void DesktopMediaListBase::SetViewDialogWindowId(DesktopMediaID dialog_id) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  view_dialog_id_ = dialog_id;
}

void DesktopMediaListBase::StartUpdating(DesktopMediaListObserver* observer) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!observer_, base::NotFatalUntil::M161);
  observer_ = observer;

  // If there is a delegated source list, it may not have been started yet.
  if (IsSourceListDelegated())
    StartDelegatedCapturer();

  // Process sources previously discovered by a call to Update().
  if (observer_) {
    for (size_t i = 0; i < sources_.size(); i++) {
      observer_->OnSourceAdded(i);
    }
  }

  CHECK(!refresh_callback_, base::NotFatalUntil::M161);
  refresh_callback_ = base::BindOnce(&DesktopMediaListBase::ScheduleNextRefresh,
                                     weak_factory_.GetWeakPtr());
  Refresh(true);
}

void DesktopMediaListBase::Update(UpdateCallback callback) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(sources_.empty(), base::NotFatalUntil::M161);
  CHECK(!refresh_callback_, base::NotFatalUntil::M161);
  refresh_callback_ = std::move(callback);
  Refresh(false);
}

int DesktopMediaListBase::GetSourceCount() const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  return sources_.size();
}

const DesktopMediaList::Source& DesktopMediaListBase::GetSource(
    int index) const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK_GE(index, 0, base::NotFatalUntil::M161);
  CHECK_LT(index, static_cast<int>(sources_.size()), base::NotFatalUntil::M161);
  return sources_[index];
}

DesktopMediaList::Type DesktopMediaListBase::GetMediaListType() const {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  return type_;
}

bool DesktopMediaListBase::IsSourceListDelegated() const {
  return false;
}

void DesktopMediaListBase::ClearDelegatedSourceListSelection() {
  NOTREACHED();
}

void DesktopMediaListBase::FocusList() {}
void DesktopMediaListBase::HideList() {}
void DesktopMediaListBase::ShowDelegatedList() {}

DesktopMediaListBase::SourceDescription::SourceDescription(
    DesktopMediaID id,
    const std::u16string& name,
    bool is_sharing_blocked)
    : id(id), name(name), is_sharing_blocked(is_sharing_blocked) {}

void DesktopMediaListBase::UpdateSourcesList(
    const std::vector<SourceDescription>& new_sources) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  typedef std::set<DesktopMediaID> SourceSet;
  SourceSet new_source_set;
  for (size_t i = 0; i < new_sources.size(); ++i) {
    new_source_set.insert(new_sources[i].id);
  }
  // Iterate through the old sources to find the removed sources.
  for (size_t i = 0; i < sources_.size(); ++i) {
    if (new_source_set.find(sources_[i].id) == new_source_set.end()) {
      sources_.erase(sources_.begin() + i);
      if (observer_)
        observer_->OnSourceRemoved(i);
      --i;
    }
  }
  // Iterate through the new sources to find the added sources.
  if (new_sources.size() > sources_.size()) {
    SourceSet old_source_set;
    for (size_t i = 0; i < sources_.size(); ++i) {
      old_source_set.insert(sources_[i].id);
    }

    for (size_t i = 0; i < new_sources.size(); ++i) {
      if (old_source_set.find(new_sources[i].id) == old_source_set.end()) {
        sources_.insert(sources_.begin() + i, Source());
        sources_[i].id = new_sources[i].id;
        sources_[i].name = new_sources[i].name;
        sources_[i].is_sharing_blocked = new_sources[i].is_sharing_blocked;
        if (observer_)
          observer_->OnSourceAdded(i);
      }
    }
  }
  CHECK_EQ(new_sources.size(), sources_.size(), base::NotFatalUntil::M161);

  // Find the moved/changed sources.
  size_t pos = 0;
  while (pos < sources_.size()) {
    if (!(sources_[pos].id == new_sources[pos].id)) {
      // Find the source that should be moved to |pos|, starting from |pos + 1|
      // of |sources_|, because entries before |pos| should have been sorted.
      size_t old_pos = pos + 1;
      for (; old_pos < sources_.size(); ++old_pos) {
        if (sources_[old_pos].id == new_sources[pos].id)
          break;
      }
      CHECK(sources_[old_pos].id == new_sources[pos].id,
            base::NotFatalUntil::M161);

      // Move the source from |old_pos| to |pos|.
      Source temp = sources_[old_pos];
      sources_.erase(sources_.begin() + old_pos);
      sources_.insert(sources_.begin() + pos, temp);

      if (observer_)
        observer_->OnSourceMoved(old_pos, pos);
    }

    if (sources_[pos].name != new_sources[pos].name) {
      sources_[pos].name = new_sources[pos].name;
      if (observer_)
        observer_->OnSourceNameChanged(pos);
    }
    if (sources_[pos].is_sharing_blocked !=
        new_sources[pos].is_sharing_blocked) {
      sources_[pos].is_sharing_blocked = new_sources[pos].is_sharing_blocked;
      if (observer_) {
        // Reuse OnSourceThumbnailChanged to refresh the table row's icon,
        // tooltip, preview state, and dialog button enablement.
        observer_->OnSourceThumbnailChanged(pos);
      }
    }
    ++pos;
  }
}

void DesktopMediaListBase::UpdateSourceThumbnail(const DesktopMediaID& id,
                                                 const gfx::ImageSkia& image) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // Unlike other methods that check can_refresh(), this one won't cause
  // OnRefreshComplete() to be called, but the caller is expected to schedule a
  // call to OnRefreshComplete() after this method and UpdateSourcePreview()
  // have been called as many times as needed, so the check is still valid.
  CHECK(can_refresh(), base::NotFatalUntil::M161);

  for (size_t i = 0; i < sources_.size(); ++i) {
    if (sources_[i].id == id) {
      sources_[i].thumbnail = image;
      if (observer_)
        observer_->OnSourceThumbnailChanged(i);
      break;
    }
  }
}

void DesktopMediaListBase::UpdateSourcePreview(const DesktopMediaID& id,
                                               const gfx::ImageSkia& image) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);

  // Unlike other methods that check can_refresh(), this one won't cause
  // OnRefreshComplete() to be called, but the caller is expected to schedule a
  // call to OnRefreshComplete() after this method and UpdateSourceThumbnail()
  // have been called as many times as needed, so the check is still valid.
  CHECK(can_refresh(), base::NotFatalUntil::M161);

  for (size_t i = 0; i < sources_.size(); ++i) {
    if (sources_[i].id == id) {
      sources_[i].preview = image;
      if (observer_)
        observer_->OnSourcePreviewChanged(i);
      break;
    }
  }
}

// static
uint32_t DesktopMediaListBase::GetImageHash(const gfx::Image& image) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  SkBitmap bitmap = image.AsBitmap();
  return base::FastHash(UNSAFE_TODO(base::span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize())));
}

void DesktopMediaListBase::OnRefreshComplete() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(refresh_callback_, base::NotFatalUntil::M161);
  std::move(refresh_callback_).Run();
}

void DesktopMediaListBase::ScheduleNextRefresh() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(!refresh_callback_, base::NotFatalUntil::M161);
  refresh_callback_ = base::BindOnce(&DesktopMediaListBase::ScheduleNextRefresh,
                                     weak_factory_.GetWeakPtr());
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DesktopMediaListBase::Refresh, weak_factory_.GetWeakPtr(),
                     true),
      update_period_);
}

void DesktopMediaListBase::OnDelegatedSourceListSelection() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(IsSourceListDelegated(), base::NotFatalUntil::M161);
  if (observer_)
    observer_->OnDelegatedSourceListSelection();

  Refresh(false);
}

void DesktopMediaListBase::OnDelegatedSourceListDismissed() {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M161);
  CHECK(IsSourceListDelegated(), base::NotFatalUntil::M161);
  if (observer_)
    observer_->OnDelegatedSourceListDismissed();

  Refresh(false);
}
