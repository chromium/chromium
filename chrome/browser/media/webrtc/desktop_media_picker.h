// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

class DesktopMediaList;

namespace content {
class WebContents;
}

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kAndroidMediaPicker);
#endif

// Base class for desktop media picker UI. It's used by Desktop Media API, and
// by ARC to let user choose a desktop media source.
//
// TODO(crbug.com/40637301): Rename this class.
class DesktopMediaPicker {
 public:
  using DoneCallback = base::OnceCallback<void(content::DesktopMediaID id)>;

  struct Params {
    // Possible sources of the request.
    enum class RequestSource {
      kUnknown,
      kCast,
      kExtension,
      kGetDisplayMedia,
      kScreenshotDataCollector,
      kArcScreenCapture,
    };

    explicit Params(RequestSource request_source);
    Params();
    Params(const Params&);
    Params& operator=(const Params&);
    ~Params();

    // WebContents this picker is relative to, can be null.
    raw_ptr<content::WebContents> web_contents = nullptr;
    // The context whose root window is used for dialog placement, cannot be
    // null for Aura.
    gfx::NativeWindow context = gfx::NativeWindow();
    // Parent window the dialog is relative to, only used on Mac.
    gfx::NativeWindow parent = gfx::NativeWindow();
    // The modality used for showing the dialog.
    ui::mojom::ModalType modality = ui::mojom::ModalType::kChild;
    // The name used in the dialog for what is requesting the picker to be
    // shown.
    std::u16string app_name;
    // Can be the same as target_name. If it is not then this is used in the
    // dialog for what is specific target within the app_name is requesting the
    // picker.
    std::u16string target_name;
    // Whether audio capture should be shown as an option in the picker.
    bool request_audio = false;
    // If audio is requested, |exclude_system_audio| can indicate that
    // system-audio should nevertheless not be offered to the user.
    // Mutually exclusive with |force_audio_checkboxes_to_default_checked|.
    bool exclude_system_audio = false;
    // Normally, the media-picker sets the default states for the audio
    // checkboxes. If |force_audio_checkboxes_to_default_checked| is |true|,
    // it sets them all to |checked|. This is used by Chromecasting.
    // It is mutually exclusive with |exclude_system_audio|.
    bool force_audio_checkboxes_to_default_checked = false;
    // Indicates that, if audio ends up being captured, then local playback
    // over the user's local speakers should be suppressed.
    bool suppress_local_audio_playback = false;
    // This flag controls the behvior in the case where the picker is invoked to
    // select a screen and there is only one screen available.  If true, the
    // dialog is bypassed entirely and the screen is automatically selected.
    // This behavior is disabled by default because in addition to letting the
    // user select a desktop, the desktop picker also serves to prevent the
    // screen screen from being shared without the user's explicit consent.
    bool select_only_screen = false;
    // Indicates that the caller of this picker is subject to enterprise
    // policies that may restrict the available choices, and a suitable warning
    // should be shown to the user.
    bool restricted_by_policy = false;
    // Indicate which display surface should be most prominently offered in the
    // picker.
    blink::mojom::PreferredDisplaySurface preferred_display_surface =
        blink::mojom::PreferredDisplaySurface::NO_PREFERENCE;
    // Indicates the source of the request. This is useful for UMA that
    // track the result of the picker, because the behavior with the
    // Extension API is different, and could therefore lead to mismeasurement.
    RequestSource request_source = RequestSource::kUnknown;
  };

  // Creates a picker dialog/confirmation box depending on the value of
  // |request|. If no request is available the default picker, namely
  // DesktopMediaPickerViews is used.
  static std::unique_ptr<DesktopMediaPicker> Create(
      const content::MediaStreamRequest* request);

  DesktopMediaPicker() = default;

  DesktopMediaPicker(const DesktopMediaPicker&) = delete;
  DesktopMediaPicker& operator=(const DesktopMediaPicker&) = delete;

  virtual ~DesktopMediaPicker() = default;

  // Shows dialog with list of desktop media sources (screens, windows, tabs)
  // provided by |sources_lists|.
  // Dialog window will call |done_callback| when user chooses one of the
  // sources or closes the dialog.
  virtual void Show(const Params& params,
                    std::vector<std::unique_ptr<DesktopMediaList>> source_lists,
                    DoneCallback done_callback) = 0;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_H_
