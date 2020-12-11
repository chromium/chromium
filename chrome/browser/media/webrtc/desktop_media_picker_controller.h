// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_CONTROLLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/ui_base_types.h"

class DesktopMediaList;
class DesktopMediaPickerFactory;

// The main entry point for the desktop picker dialog box, which prompts the
// user to select a desktop or an application window whose content will be made
// available as a video stream.
//
// TODO(crbug.com/987001): Rename this class.  Consider merging with
// DesktopMediaPickerViews and naming the merged class just DesktopMediaPicker.
class DesktopMediaPickerController : private content::WebContentsObserver {
 public:
  using Params = DesktopMediaPicker::Params;

  // Callback for desktop selection results.  There are three possible cases:
  //
  // - If |err| is non-empty, it contains an error message regarding why the
  //   dialog could not be displayed, and the value of |id| should not be used.
  //
  // - If |err| is empty and id.is_null() is true, the user canceled the dialog.
  //
  // - Otherwise, |id| represents the user's selection.
  using DoneCallback = base::OnceCallback<void(const std::string& err,
                                               content::DesktopMediaID id)>;

  explicit DesktopMediaPickerController(
      DesktopMediaPickerFactory* picker_factory = nullptr);
  DesktopMediaPickerController(const DesktopMediaPickerController&) = delete;
  DesktopMediaPickerController& operator=(const DesktopMediaPickerController&) =
      delete;
  ~DesktopMediaPickerController() override;

  // Show the desktop picker dialog using the parameters specified by |params|,
  // with the possible selections restricted to those included in |sources|.  If
  // an error is detected synchronously, it is reported by returning an error
  // string.  Otherwise, the return value is nullopt, and the closure passed as
  // |done_callback| is called when the dialog is closed.  If the dialog is
  // canceled, the argument to |done_callback| will be an instance of
  // DesktopMediaID whose is_null() method returns true.
  //
  // As a special case, if |params.select_only_screen| is true, and the only
  // selection type is TYPE_SCREEN, and there is only one screen,
  // |done_callback| is called immediately with the screen's ID, and the dialog
  // is not shown.  This option must be used with care, because even when the
  // dialog has only one option to select, the dialog itself helps prevent the
  // user for accidentally sharing their screen and gives them the option to
  // prevent their screen from being shared.
  //
  // Note that |done_callback| is called only if the dialog completes normally.
  // If an instance of this class is destroyed while the dialog is visible, the
  // dialog will be cleaned up, but |done_callback| will not be invoked.
  void Show(const Params& params,
            const std::vector<content::DesktopMediaID::Type>& sources,
            DoneCallback done_callback);

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override;

 private:
  void OnInitialMediaListFound();
  void ShowPickerDialog();
  // This function is responsible to call |done_callback_| and after running the
  // callback |this| might be destroyed. Do **not** access fields after calling
  // this function.
  void OnPickerDialogResults(const std::string& err,
                             content::DesktopMediaID source);

  Params params_;
  DoneCallback done_callback_;
  std::vector<std::unique_ptr<DesktopMediaList>> source_lists_;
  std::unique_ptr<DesktopMediaPicker> picker_;
  DesktopMediaPickerFactory* picker_factory_;
  base::WeakPtrFactory<DesktopMediaPickerController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_DESKTOP_MEDIA_PICKER_CONTROLLER_H_
