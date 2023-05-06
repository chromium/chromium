// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_FACTORY_IMPL_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_FACTORY_IMPL_H_

#include <memory>
#include <vector>

#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"

// Factory creating DesktopMediaList and DesktopMediaPicker instances.
class DesktopMediaPickerFactoryImpl : public DesktopMediaPickerFactory {
 public:
  DesktopMediaPickerFactoryImpl();

  DesktopMediaPickerFactoryImpl(const DesktopMediaPickerFactoryImpl&) = delete;
  DesktopMediaPickerFactoryImpl& operator=(
      const DesktopMediaPickerFactoryImpl&) = delete;

  ~DesktopMediaPickerFactoryImpl() override;

  // Get the lazy initialized instance of the factory.
  static DesktopMediaPickerFactoryImpl* GetInstance();

  // DesktopMediaPickerFactory implementation
  // Can return |nullptr| if platform doesn't support DesktopMediaPicker.
  std::unique_ptr<DesktopMediaPicker> CreatePicker(
      const content::MediaStreamRequest* request) override;

  std::vector<std::unique_ptr<DesktopMediaList>> CreateMediaList(
      const std::vector<DesktopMediaList::Type>& types,
      content::WebContents* web_contents,
      DesktopMediaList::WebContentsFilter includable_web_contents_filter)
      override;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_FACTORY_IMPL_H_
