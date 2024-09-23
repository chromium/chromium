// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_media_picker_factory_impl.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/media/webrtc/current_tab_desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_capturer_wrapper.h"
#include "chrome/browser/media/webrtc/native_desktop_media_list.h"
#include "chrome/browser/media/webrtc/tab_desktop_media_list.h"
#include "content/public/browser/desktop_capture.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/thumbnail_capturer_mac.h"
#endif

namespace {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<ThumbnailCapturer> MakeScreenCapturer() {
#if BUILDFLAG(IS_MAC)
  if (ShouldUseThumbnailCapturerMac(DesktopMediaList::Type::kScreen)) {
    return CreateThumbnailCapturerMac(DesktopMediaList::Type::kScreen);
  }
#endif  // BUILDFLAG(IS_MAC)

  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer =
      content::desktop_capture::CreateScreenCapturer();
  return desktop_capturer ? std::make_unique<DesktopCapturerWrapper>(
                                std::move(desktop_capturer))
                          : nullptr;
}

std::unique_ptr<ThumbnailCapturer> MakeWindowCapturer() {
#if BUILDFLAG(IS_MAC)
  if (ShouldUseThumbnailCapturerMac(DesktopMediaList::Type::kWindow)) {
    return CreateThumbnailCapturerMac(DesktopMediaList::Type::kWindow);
  }
#endif  // BUILDFLAG(IS_MAC)

  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer =
      content::desktop_capture::CreateWindowCapturer();
  return desktop_capturer ? std::make_unique<DesktopCapturerWrapper>(
                                std::move(desktop_capturer))
                          : nullptr;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

DesktopMediaPickerFactoryImpl::DesktopMediaPickerFactoryImpl() = default;

DesktopMediaPickerFactoryImpl::~DesktopMediaPickerFactoryImpl() = default;

// static
DesktopMediaPickerFactoryImpl* DesktopMediaPickerFactoryImpl::GetInstance() {
  static base::NoDestructor<DesktopMediaPickerFactoryImpl> impl;
  return impl.get();
}

std::unique_ptr<DesktopMediaPicker> DesktopMediaPickerFactoryImpl::CreatePicker(
    const content::MediaStreamRequest* request) {
// DesktopMediaPicker is implemented only for Windows, OSX and Aura Linux
// builds.
#if defined(TOOLKIT_VIEWS)
  return DesktopMediaPicker::Create(request);
#else
  return nullptr;
#endif
}

std::vector<std::unique_ptr<DesktopMediaList>>
DesktopMediaPickerFactoryImpl::CreateMediaList(
    const std::vector<DesktopMediaList::Type>& types,
    content::WebContents* web_contents,
    DesktopMediaList::WebContentsFilter includable_web_contents_filter) {
  // If we're supposed to include Tabs, but aren't including Windows (either
  // directly or indirectly), then we need to add Chrome App Windows back in.
  const bool add_chrome_app_windows =
      !base::Contains(types, DesktopMediaList::Type::kWindow) &&
      base::Contains(types, DesktopMediaList::Type::kWebContents);
  // Keep same order as the input |sources| and avoid duplicates.
  std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
  bool have_screen_list = false;
  bool have_window_list = false;
  bool have_tab_list = false;
  bool have_current_tab = false;

  for (auto source_type : types) {
    switch (source_type) {
      case DesktopMediaList::Type::kNone:
        break;
      case DesktopMediaList::Type::kScreen: {
        if (have_screen_list)
          continue;
        std::unique_ptr<DesktopMediaList> screen_list;
#if BUILDFLAG(IS_CHROMEOS_ASH)
        screen_list = std::make_unique<DesktopMediaListAsh>(
            DesktopMediaList::Type::kScreen);
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
        // If screen capture is not supported on the platform, then we should
        // not attempt to create an instance of NativeDesktopMediaList. Doing so
        // will hit a DCHECK.
        std::unique_ptr<ThumbnailCapturer> capturer = MakeScreenCapturer();
        if (!capturer)
          continue;

#if BUILDFLAG(IS_MAC)
        const bool auto_show_delegated_source_list = false;
#else
        const bool auto_show_delegated_source_list = true;
#endif  // BUILDFLAG(IS_MAC)
        screen_list = std::make_unique<NativeDesktopMediaList>(
            DesktopMediaList::Type::kScreen, std::move(capturer),
            /*add_current_process_windows=*/false,
            auto_show_delegated_source_list);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
        have_screen_list = true;
        source_lists.push_back(std::move(screen_list));
        break;
      }
      case DesktopMediaList::Type::kWindow: {
        if (have_window_list)
          continue;
        std::unique_ptr<DesktopMediaList> window_list;
#if BUILDFLAG(IS_CHROMEOS_ASH)
        window_list = std::make_unique<DesktopMediaListAsh>(
            DesktopMediaList::Type::kWindow);
#else   // !BUILDFLAG(IS_CHROMEOS_ASH)
        // If window capture is not supported on the platform, then we should
        // not attempt to create an instance of NativeDesktopMediaList. Doing so
        // will hit a DCHECK.
        std::unique_ptr<ThumbnailCapturer> capturer = MakeWindowCapturer();
        if (!capturer)
          continue;
        // If the capturer is not going to enumerate current process windows
        // (to avoid a deadlock on Windows), then we have to find and add those
        // windows ourselves.
        const bool add_current_process_windows =
            !content::desktop_capture::ShouldEnumerateCurrentProcessWindows();
#if BUILDFLAG(IS_MAC)
        const bool auto_show_delegated_source_list = false;
#else
        const bool auto_show_delegated_source_list = true;
#endif  // BUILDFLAG(IS_MAC)
        window_list = std::make_unique<NativeDesktopMediaList>(
            DesktopMediaList::Type::kWindow, std::move(capturer),
            add_current_process_windows, auto_show_delegated_source_list);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
        have_window_list = true;
        source_lists.push_back(std::move(window_list));
        break;
      }
      case DesktopMediaList::Type::kWebContents: {
        if (have_tab_list)
          continue;
        // Since the TabDesktopMediaList is the only MediaList that uses the
        // web contents filter, and we explicitly skip this if we already have
        // one, the std::move here is safe.
        std::unique_ptr<DesktopMediaList> tab_list =
            std::make_unique<TabDesktopMediaList>(
                web_contents, std::move(includable_web_contents_filter),
                add_chrome_app_windows);
        have_tab_list = true;
        source_lists.push_back(std::move(tab_list));
        break;
      }
      case DesktopMediaList::Type::kCurrentTab: {
        if (have_current_tab)
          continue;
        have_current_tab = true;
        source_lists.push_back(
            std::make_unique<CurrentTabDesktopMediaList>(web_contents));
        break;
      }
    }
  }
  return source_lists;
}
