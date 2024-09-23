// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPPORT_TOOL_SCREENSHOT_DATA_COLLECTOR_H_
#define CHROME_BROWSER_SUPPORT_TOOL_SCREENSHOT_DATA_COLLECTOR_H_

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "content/public/browser/desktop_capture.h"
#include "content/public/browser/desktop_media_id.h"

// ScreenshotDataCollector opens a DesktopMediaPicker dialogue so that the user
// can choose to take a screenshot of the entire screen, a window, or a tab.
// This screenshot will be included in the exported Support Tool package.
class ScreenshotDataCollector : public DataCollector,
                                public webrtc::DesktopCapturer::Callback {
 public:
  ScreenshotDataCollector();
  ~ScreenshotDataCollector() override;

  ScreenshotDataCollector(const ScreenshotDataCollector&) = delete;
  ScreenshotDataCollector& operator=(const ScreenshotDataCollector&) = delete;
  ScreenshotDataCollector(ScreenshotDataCollector&&) = delete;
  ScreenshotDataCollector& operator=(ScreenshotDataCollector&&) = delete;

  // Returns the captured image in base64 encoding, if succeeded.
  std::string GetScreenshotBase64();

  // Updates the captured image with a new image (with sensitive info masked,
  // etc.).
  void SetScreenshotBase64(std::string screenshot_base64);

  // Overrides from DataCollector.
  std::string GetName() const override;

  std::string GetDescription() const override;

  const PIIMap& GetDetectedPII() override;

  void CollectDataAndDetectPII(
      DataCollectorDoneCallback on_data_collected_callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container)
      override;

  void ExportCollectedDataWithPII(
      std::set<redaction::PIIType> pii_types_to_keep,
      base::FilePath target_directory,
      scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool,
      scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container,
      DataCollectorDoneCallback on_exported_callback) override;

  // Override when webrtc::DesktopCapturer is used.
  void OnCaptureResult(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame) override;

  // Converts a screenshot in the format of `webrtc::DesktopFrame` to a
  // base64-encoded jpeg image. Sets `image_base64` to be the resultant string.
  void ConvertDesktopFrameToBase64JPEG(
      std::unique_ptr<webrtc::DesktopFrame> frame,
      std::string& image_base64);

  // Sets `picker_factory_for_testing_`. Used only in the browser test.
  void SetPickerFactoryForTesting(
      DesktopMediaPickerFactory* picker_factory_ptr);

 private:
  // Is called when the user has selected a source to be captured. Performs a
  // capture on that source.
  void OnSourceSelected(const std::string& err, content::DesktopMediaID id);

#if BUILDFLAG(IS_CHROMEOS)
  // Is called when a screenshot attempt has been made by
  // ui::GrabWindowSnapshotAsJPEG. Stores the captured image in
  // `screenshot_base64_`.
  void OnScreenshotTaken(scoped_refptr<base::RefCountedMemory> data);
#else
  // Is called when a tab has been captured. Encodes the screenshot to JPEG and
  // stores it in `data_`.
  void OnTabCaptured(const SkBitmap& bitmap);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // Is called when the screenshot has been exported (or failed to be exported).
  void OnScreenshotExported(bool success);

  SEQUENCE_CHECKER(sequence_checker_);
  PIIMap pii_map_;
  // Dialogue to pick the capture source.
  std::unique_ptr<DesktopMediaPickerController> picker_controller_;
  // Pointer to a FakeDesktopMediaPickerFactory. Used only in testing.
  raw_ptr<DesktopMediaPickerFactory> picker_factory_for_testing_ = nullptr;
  // Capturer for screens and windows.
  std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;
  // Base64-encoded image.
  std::string screenshot_base64_;
  DataCollectorDoneCallback data_collector_done_callback_;
  base::WeakPtrFactory<ScreenshotDataCollector> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_SUPPORT_TOOL_SCREENSHOT_DATA_COLLECTOR_H_
