// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_DELEGATE_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace aura {
class Window;
}  // namespace aura

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
namespace mojom {
class AudioStreamFactory;
}  // namespace mojom
}  // namespace media

namespace recording {
namespace mojom {
class RecordingService;
}  // namespace mojom
}  // namespace recording

namespace ash {

class RecordingOverlayView;

// Defines the interface for the delegate of CaptureModeController, that can be
// implemented by an ash client (e.g. Chrome). The CaptureModeController owns
// the instance of this delegate.
class ASH_PUBLIC_EXPORT CaptureModeDelegate {
 public:
  virtual ~CaptureModeDelegate() = default;

  // Returns the path to the default downloads directory of the currently active
  // user. This function can only be called if the user is logged in.
  virtual base::FilePath GetUserDefaultDownloadsFolder() const = 0;

  // Shows the screenshot or screen recording item in the screen capture folder.
  virtual void ShowScreenCaptureItemInFolder(
      const base::FilePath& file_path) = 0;

  // Opens the screenshot item in an image editor.
  virtual void OpenScreenshotInImageEditor(const base::FilePath& file_path) = 0;

  // Returns true if the current user is using the 24-hour format (i.e. 14:00
  // vs. 2:00 PM). This is used to build the file name of the captured image or
  // video.
  virtual bool Uses24HourFormat() const = 0;

  // Returns whether initiation of capture mode is restricted because of Data
  // Leak Prevention applied to the currently visible content.
  virtual bool IsCaptureModeInitRestrictedByDlp() const = 0;

  // Returns whether capture of the region defined by |window| and |bounds|
  // is currently allowed by Data Leak Prevention feature.
  virtual bool IsCaptureAllowedByDlp(const aura::Window* window,
                                     const gfx::Rect& bounds,
                                     bool for_video) const = 0;

  // Returns whether screen capture is allowed by an enterprise policy.
  virtual bool IsCaptureAllowedByPolicy() const = 0;

  // Called when a video capture for |window| and |bounds| area is started, so
  // that Data Leak Prevention can start observing the area.
  // |on_area_restricted_callback| will be called when the area becomes
  // restricted so that the capture should be interrupted.
  virtual void StartObservingRestrictedContent(
      const aura::Window* window,
      const gfx::Rect& bounds,
      base::OnceClosure on_area_restricted_callback) = 0;

  // Called when the running video capture is stopped.
  virtual void StopObservingRestrictedContent() = 0;

  // Launches the Recording Service into a separate utility process.
  virtual mojo::Remote<recording::mojom::RecordingService>
  LaunchRecordingService() = 0;

  // Binds the given AudioStreamFactory |receiver| to the audio service.
  virtual void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) = 0;

  // Called when a capture mode session starts or stops.
  virtual void OnSessionStateChanged(bool started) = 0;

  // Called after the controller resets its |mojo::Remote| instance of the
  // service.
  virtual void OnServiceRemoteReset() = 0;

  // Creates and returns the view that will be used as the contents view of the
  // overlay widget, which is added as a child of the recorded surface to host
  // contents rendered in a web view that are meant to be part of the recording
  // such as annotations.
  virtual std::unique_ptr<RecordingOverlayView> CreateRecordingOverlayView()
      const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_DELEGATE_H_
