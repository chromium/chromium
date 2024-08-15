// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media/webrtc/desktop_media_list_base.h"

#include <set>
#include <utility>

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DesktopMediaListBase::DesktopMediaListBase(base::TimeDelta update_period,
                                           DesktopMediaListObserver* observer)
    : update_period_(update_period), observer_(observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DesktopMediaListBase::~DesktopMediaListBase() = default;

void DesktopMediaListBase::SetUpdatePeriod(base::TimeDelta period) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!observer_);
  update_period_ = period;
}

void DesktopMediaListBase::SetThumbnailSize(const gfx::Size& thumbnail_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  thumbnail_size_ = thumbnail_size;
}

void DesktopMediaListBase::SetViewDialogWindowId(DesktopMediaID dialog_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  view_dialog_id_ = dialog_id;
}

void DesktopMediaListBase::StartUpdating(DesktopMediaListObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!observer_);
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

  DCHECK(!refresh_callback_);
  refresh_callback_ = base::BindOnce(&DesktopMediaListBase::ScheduleNextRefresh,
                                     weak_factory_.GetWeakPtr());
  Refresh(true);
}

void DesktopMediaListBase::Update(UpdateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(sources_.empty());
  DCHECK(!refresh_callback_);
  refresh_callback_ = std::move(callback);
  Refresh(false);
}

int DesktopMediaListBase::GetSourceCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return sources_.size();
}

const DesktopMediaList::Source& DesktopMediaListBase::GetSource(
    int index) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(sources_.size()));
  return sources_[index];
}

DesktopMediaList::Type DesktopMediaListBase::GetMediaListType() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return type_;
}

bool DesktopMediaListBase::IsSourceListDelegated() const {
  return false;
}

void DesktopMediaListBase::ClearDelegatedSourceListSelection() {
  NOTREACHED_IN_MIGRATION();
}

void DesktopMediaListBase::FocusList() {}
void DesktopMediaListBase::HideList() {}
void DesktopMediaListBase::ShowDelegatedList() {}

DesktopMediaListBase::SourceDescription::SourceDescription(
    DesktopMediaID id,
    const std::u16string& name)
    : id(id), name(name) {}

void DesktopMediaListBase::UpdateSourcesList(
    const std::vector<SourceDescription>& new_sources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
        if (observer_)
          observer_->OnSourceAdded(i);
      }
    }
  }
  DCHECK_EQ(new_sources.size(), sources_.size());

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
      DCHECK(sources_[old_pos].id == new_sources[pos].id);

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
    ++pos;
  }
}

void DesktopMediaListBase::UpdateSourceThumbnail(const DesktopMediaID& id,
                                                 const gfx::ImageSkia& image) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Unlike other methods that check can_refresh(), this one won't cause
  // OnRefreshComplete() to be called, but the caller is expected to schedule a
  // call to OnRefreshComplete() after this method and UpdateSourcePreview()
  // have been called as many times as needed, so the check is still valid.
  DCHECK(can_refresh());

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Unlike other methods that check can_refresh(), this one won't cause
  // OnRefreshComplete() to be called, but the caller is expected to schedule a
  // call to OnRefreshComplete() after this method and UpdateSourceThumbnail()
  // have been called as many times as needed, so the check is still valid.
  DCHECK(can_refresh());

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SkBitmap bitmap = image.AsBitmap();
  return base::FastHash(base::make_span(
      static_cast<uint8_t*>(bitmap.getPixels()), bitmap.computeByteSize()));
}

void DesktopMediaListBase::OnRefreshComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(refresh_callback_);
  std::move(refresh_callback_).Run();
}

void DesktopMediaListBase::ScheduleNextRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!refresh_callback_);
  refresh_callback_ = base::BindOnce(&DesktopMediaListBase::ScheduleNextRefresh,
                                     weak_factory_.GetWeakPtr());
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DesktopMediaListBase::Refresh, weak_factory_.GetWeakPtr(),
                     true),
      update_period_);
}

void DesktopMediaListBase::OnDelegatedSourceListSelection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsSourceListDelegated());
  if (observer_)
    observer_->OnDelegatedSourceListSelection();

  Refresh(false);
}

void DesktopMediaListBase::OnDelegatedSourceListDismissed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsSourceListDelegated());
  if (observer_)
    observer_->OnDelegatedSourceListDismissed();

  Refresh(false);
}
