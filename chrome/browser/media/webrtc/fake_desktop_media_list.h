// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_LIST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_LIST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"

class FakeDesktopMediaList : public DesktopMediaList {
 public:
  explicit FakeDesktopMediaList(DesktopMediaList::Type type,
                                bool is_source_list_delegated = false);

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
  void OnDelegatedSourceListSelection();
  void OnDelegatedSourceListDismissed();

  bool is_focused() const { return is_focused_; }
  int clear_delegated_source_list_selection_count() const {
    return clear_delegated_source_list_selection_count_;
  }

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
      const std::optional<content::DesktopMediaID>& id) override;
  bool IsSourceListDelegated() const override;
  void ClearDelegatedSourceListSelection() override;
  void FocusList() override;
  void HideList() override;
  void ShowDelegatedList() override;

 private:
  std::vector<Source> sources_;
  raw_ptr<DesktopMediaListObserver> observer_ = nullptr;
  gfx::ImageSkia thumbnail_;
  const DesktopMediaList::Type type_;
  const bool is_source_list_delegated_;
  bool is_focused_ = false;
  int clear_delegated_source_list_selection_count_ = 0;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_FAKE_DESKTOP_MEDIA_LIST_H_
