// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/fake_desktop_media_list.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_list_observer.h"
#include "ui/gfx/skia_util.h"

using content::DesktopMediaID;

FakeDesktopMediaList::FakeDesktopMediaList(DesktopMediaID::Type type)
    : observer_(NULL), type_(type) {}
FakeDesktopMediaList::~FakeDesktopMediaList() {}

void FakeDesktopMediaList::AddSource(int id) {
  AddSourceByFullMediaID(
      content::DesktopMediaID(DesktopMediaID::TYPE_WINDOW, id));
}

void FakeDesktopMediaList::AddSourceByFullMediaID(
    content::DesktopMediaID media_id) {
  Source source;
  source.id = media_id;
  source.name = base::NumberToString16(media_id.id);

  sources_.push_back(source);
  observer_->OnSourceAdded(this, sources_.size() - 1);
}

void FakeDesktopMediaList::RemoveSource(int index) {
  sources_.erase(sources_.begin() + index);
  observer_->OnSourceRemoved(this, index);
}

void FakeDesktopMediaList::MoveSource(int old_index, int new_index) {
  Source source = sources_[old_index];
  sources_.erase(sources_.begin() + old_index);
  sources_.insert(sources_.begin() + new_index, source);
  observer_->OnSourceMoved(this, old_index, new_index);
}

void FakeDesktopMediaList::SetSourceThumbnail(int index) {
  sources_[index].thumbnail = thumbnail_;
  observer_->OnSourceThumbnailChanged(this, index);
}

void FakeDesktopMediaList::SetSourceName(int index, base::string16 name) {
  sources_[index].name = name;
  observer_->OnSourceNameChanged(this, index);
}

void FakeDesktopMediaList::SetUpdatePeriod(base::TimeDelta period) {}

void FakeDesktopMediaList::SetThumbnailSize(const gfx::Size& thumbnail_size) {}

void FakeDesktopMediaList::SetViewDialogWindowId(
    content::DesktopMediaID dialog_id) {}

void FakeDesktopMediaList::StartUpdating(DesktopMediaListObserver* observer) {
  observer_ = observer;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(150, 150);
  bitmap.eraseARGB(255, 0, 255, 0);
  thumbnail_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

void FakeDesktopMediaList::Update(UpdateCallback callback) {
  std::move(callback).Run();
}

int FakeDesktopMediaList::GetSourceCount() const {
  return sources_.size();
}

const DesktopMediaList::Source& FakeDesktopMediaList::GetSource(
    int index) const {
  return sources_[index];
}

DesktopMediaID::Type FakeDesktopMediaList::GetMediaListType() const {
  return type_;
}
