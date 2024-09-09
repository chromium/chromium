// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DELEGATED_SOURCE_LIST_CAPTURER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DELEGATED_SOURCE_LIST_CAPTURER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

// This class is a DesktopCapturer that fully delegates the picking of a source
// to a device specific picker by implementing the DelegatedSourceListController
// interface and calling the device specific picker.
class DelegatedSourceListCapturer
    : public webrtc::DesktopCapturer,
      public webrtc::DelegatedSourceListController {
 public:
  explicit DelegatedSourceListCapturer(content::DesktopMediaID::Type type);
  ~DelegatedSourceListCapturer() override;

  // DesktopCapturer
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  webrtc::DelegatedSourceListController* GetDelegatedSourceListController()
      override;

  // DelegatedSourceListController
  void Observe(Observer* observer) override;
  void EnsureVisible() override;
  void EnsureHidden() override;

 private:
  void OnPickerCreated(content::DesktopMediaID::Id source_id);
  void OnSelected(Source source);
  void OnCancelled();
  void OnError();

  raw_ptr<Observer> delegated_source_list_observer_ = nullptr;
  std::optional<Source> selected_source_;
  std::optional<content::DesktopMediaID::Id> source_id_;
  content::DesktopMediaID::Type type_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DelegatedSourceListCapturer> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DELEGATED_SOURCE_LIST_CAPTURER_H_
