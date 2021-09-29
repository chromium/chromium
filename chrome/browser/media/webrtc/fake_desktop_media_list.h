// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_LIST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"

class FakeDesktopMediaList : public DesktopMediaList {
 public:
  explicit FakeDesktopMediaList(DesktopMediaList::Type type);

  FakeDesktopMediaList(const FakeDesktopMediaList&) = delete;
  FakeDesktopMediaList& operator=(const FakeDesktopMediaList&) = delete;

  ~FakeDesktopMediaList() override;

  void AddSource(int id);
  void AddSourceByFullMediaID(content::DesktopMediaID media_id);
  void RemoveSource(int index);
  void MoveSource(int old_index, int new_index);
  void SetSourceThumbnail(int index);
  void SetSourceName(int index, std::u16string name);
  void SetSourcePreview(int index, gfx::ImageSkia);

  // DesktopMediaList implementation:
  void SetUpdatePeriod(base::TimeDelta period) override;
  void SetThumbnailSize(const gfx::Size& thumbnail_size) override;
  void SetViewDialogWindowId(content::DesktopMediaID dialog_id) override;
  void StartUpdating(DesktopMediaListObserver* observer) override;
  void Update(UpdateCallback callback) override;
  int GetSourceCount() const override;
  const Source& GetSource(int index) const override;
  DesktopMediaList::Type GetMediaListType() const override;
  void SetPreviewedSource(
      const absl::optional<content::DesktopMediaID>& id) override;

 private:
  std::vector<Source> sources_;
  raw_ptr<DesktopMediaListObserver> observer_;
  gfx::ImageSkia thumbnail_;
  const DesktopMediaList::Type type_;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_LIST_H_
