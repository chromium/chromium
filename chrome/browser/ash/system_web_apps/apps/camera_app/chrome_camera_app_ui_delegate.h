// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_APP_UI_DELEGATE_H_

#include <memory>

#include "ash/webui/camera_app_ui/camera_app_ui_delegate.h"
#include "ash/webui/camera_app_ui/pdf_builder.mojom.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/services/pdf/public/mojom/pdf_progressive_searchifier.mojom.h"
#include "chrome/services/pdf/public/mojom/pdf_service.mojom.h"
#include "chrome/services/pdf/public/mojom/pdf_thumbnailer.mojom.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {
struct MediaStreamRequest;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace blink {
namespace mojom {
enum class MediaStreamType;
}  // namespace mojom
}  // namespace blink

/**
 * Implementation of the CameraAppUIDelegate interface. Provides the camera app
 * code in ash/ with functions that only exist in chrome/.
 */
class ChromeCameraAppUIDelegate : public ash::CameraAppUIDelegate {
 public:
  class CameraAppDialog : public ash::SystemWebDialogDelegate {
   public:
    CameraAppDialog(const CameraAppDialog&) = delete;
    CameraAppDialog& operator=(const CameraAppDialog&) = delete;

    static void ShowIntent(const std::string& queries,
                           gfx::NativeWindow parent);

    // SystemWebDialogDelegate
    void AdjustWidgetInitParams(views::Widget::InitParams* params) override;

    // ui::WebDialogDelegate
    void RequestMediaAccessPermission(
        content::WebContents* web_contents,
        const content::MediaStreamRequest& request,
        content::MediaResponseCallback callback) override;
    bool CheckMediaAccessPermission(
        content::RenderFrameHost* render_frame_host,
        const url::Origin& security_origin,
        blink::mojom::MediaStreamType type) override;

   private:
    explicit CameraAppDialog(const std::string& url);
    ~CameraAppDialog() override;
  };

  class FileMonitor {
   public:
    FileMonitor();
    FileMonitor(const FileMonitor&) = delete;
    FileMonitor& operator=(const FileMonitor&) = delete;
    ~FileMonitor();

    void Monitor(const base::FilePath& file_path,
                 base::OnceCallback<void(FileMonitorResult)> callback);

   private:
    void OnFileDeletion(const base::FilePath& path, bool error);

    // Things which might be touched by the callback of |file_watcher_| should
    // be destroyed later than the destruction of |file_watcher_|.
    base::OnceCallback<void(FileMonitorResult)> callback_;
    std::unique_ptr<base::FilePathWatcher> file_watcher_;
  };

  class StorageMonitor {
   public:
    explicit StorageMonitor(
        scoped_refptr<base::SequencedTaskRunner> task_runner);
    StorageMonitor(const StorageMonitor&) = delete;
    StorageMonitor& operator=(const StorageMonitor&) = delete;
    ~StorageMonitor();
    void StartMonitoring(
        base::FilePath monitor_path,
        base::RepeatingCallback<void(StorageMonitorStatus)> callback);
    void StopMonitoring();
    base::WeakPtr<ChromeCameraAppUIDelegate::StorageMonitor> GetWeakPtr();

   private:
    StorageMonitorStatus GetCurrentStatus();
    void MonitorCurrentStatus();

    base::RepeatingCallback<void(StorageMonitorStatus)> callback_;
    base::RepeatingTimer timer_;
    base::FilePath monitor_path_;
    StorageMonitorStatus status_;
    scoped_refptr<base::SequencedTaskRunner> task_runner_;
    base::WeakPtrFactory<ChromeCameraAppUIDelegate::StorageMonitor>
        weak_factory_{this};
  };

  class PdfServiceManager : public pdf::mojom::Ocr {
   public:
    explicit PdfServiceManager(
        scoped_refptr<screen_ai::OpticalCharacterRecognizer>
            optical_character_recognizer);
    PdfServiceManager(const PdfServiceManager&) = delete;
    PdfServiceManager& operator=(const PdfServiceManager&) = delete;
    ~PdfServiceManager() override;

    class ProgressivePdf : public ash::camera_app::mojom::PdfBuilder {
     public:
      ProgressivePdf(
          mojo::Remote<pdf::mojom::PdfService> pdf_service,
          mojo::Remote<pdf::mojom::PdfProgressiveSearchifier> pdf_searchifier);
      ProgressivePdf(const ProgressivePdf&) = delete;
      ProgressivePdf& operator=(const ProgressivePdf&) = delete;
      ~ProgressivePdf() override;

      // ash::camera_app::mojom::PdfBuilder
      void AddPage(mojo_base::BigBuffer jpg, uint32_t index) override;
      void AddPageInline(const std::vector<uint8_t>& jpg,
                         uint32_t index) override;
      void DeletePage(uint32_t index) override;
      void Save(SaveCallback callback) override;
      void SaveInline(SaveInlineCallback callback) override;

     private:
      void AddPageInternal(base::span<const uint8_t> jpg, uint32_t index);
      void ConsumeSaveCallback(const std::vector<uint8_t>& searchified_pdf);

      SaveInlineCallback save_callback_;
      mojo::Remote<pdf::mojom::PdfService> pdf_service_;
      mojo::Remote<pdf::mojom::PdfProgressiveSearchifier> pdf_searchifier_;
      base::WeakPtrFactory<ProgressivePdf> weak_factory_{this};
    };

    void GetThumbnail(
        const std::vector<uint8_t>& pdf,
        base::OnceCallback<void(const std::vector<uint8_t>&)> callback);
    std::unique_ptr<ProgressivePdf> CreateProgressivePdf();

   private:
    void GotThumbnail(mojo::RemoteSetElementId pdf_service_id,
                      mojo::RemoteSetElementId pdf_thumbnailer_id,
                      const SkBitmap& bitmap);
    void ConsumeGotThumbnailCallback(const std::vector<uint8_t>& thumbnail,
                                     mojo::RemoteSetElementId id);
    mojo::PendingRemote<pdf::mojom::Ocr> CreateOcrRemote();

    //  pdf::mojom::Ocr
    void PerformOcr(const SkBitmap& image,
                    PerformOcrCallback callback) override;

    mojo::RemoteSet<pdf::mojom::PdfThumbnailer> pdf_thumbnailers_;
    base::flat_map<mojo::RemoteSetElementId,
                   base::OnceCallback<void(const std::vector<uint8_t>&)>>
        pdf_thumbnailer_callbacks;

    mojo::ReceiverSet<pdf::mojom::Ocr> ocr_receivers_;
    scoped_refptr<screen_ai::OpticalCharacterRecognizer>
        optical_character_recognizer_;

    mojo::RemoteSet<pdf::mojom::PdfService> pdf_services_;
    base::WeakPtrFactory<PdfServiceManager> weak_factory_{this};
  };

  explicit ChromeCameraAppUIDelegate(content::WebUI* web_ui);

  ChromeCameraAppUIDelegate(const ChromeCameraAppUIDelegate&) = delete;
  ChromeCameraAppUIDelegate& operator=(const ChromeCameraAppUIDelegate&) =
      delete;
  ~ChromeCameraAppUIDelegate() override;

  // ash::CameraAppUIDelegate
  void SetLaunchDirectory() override;
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
  bool IsMetricsAndCrashReportingEnabled() override;
  void OpenFileInGallery(const std::string& name) override;
  void OpenFeedbackDialog(const std::string& placeholder) override;
  std::string GetFilePathInArcByName(const std::string& name) override;
  void OpenDevToolsWindow(content::WebContents* web_contents) override;
  void MonitorFileDeletion(
      const std::string& name,
      base::OnceCallback<void(FileMonitorResult)> callback) override;
  void MaybeTriggerSurvey() override;
  void StartStorageMonitor(base::RepeatingCallback<void(StorageMonitorStatus)>
                               monitor_callback) override;
  void StopStorageMonitor() override;
  void OpenStorageManagement() override;
  base::FilePath GetFilePathByName(const std::string& name) override;
  media_device_salt::MediaDeviceSaltService* GetMediaDeviceSaltService(
      content::BrowserContext* context) override;
  void OpenWifiDialog(WifiConfig wifi_config) override;
  std::string GetSystemLanguage() override;
  void RenderPdfAsJpeg(
      const std::vector<uint8_t>& pdf,
      base::OnceCallback<void(const std::vector<uint8_t>&)> callback) override;
  void PerformOcr(base::span<const uint8_t> jpeg_data,
                  base::OnceCallback<void(ash::camera_app::mojom::OcrResultPtr)>
                      callback) override;
  void CreatePdfBuilder(
      mojo::PendingReceiver<ash::camera_app::mojom::PdfBuilder> receiver)
      override;

 private:
  base::FilePath GetMyFilesFolder();
  void OnFileMonitorInitialized(std::unique_ptr<FileMonitor> file_monitor);
  void MonitorFileDeletionOnFileThread(
      FileMonitor* file_monitor,
      const base::FilePath& file_path,
      base::OnceCallback<void(FileMonitorResult)> callback);

  void InitializeStorageMonitor();
  void OnStorageMonitorInitialized(std::unique_ptr<StorageMonitor> monitor);

  raw_ptr<content::WebUI> web_ui_;  // Owns |this|.

  base::Time session_start_time_;

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  // It should only be created, used and destroyed on |file_task_runner_|.
  std::unique_ptr<FileMonitor> file_monitor_;

  // Storage monitor running on separate task runner.
  scoped_refptr<base::SequencedTaskRunner> storage_task_runner_;
  std::unique_ptr<StorageMonitor> storage_monitor_;
  base::WeakPtr<ChromeCameraAppUIDelegate::StorageMonitor>
      storage_monitor_weak_ptr_;

  std::unique_ptr<PdfServiceManager> pdf_service_manager_;

  scoped_refptr<screen_ai::OpticalCharacterRecognizer>
      optical_character_recognizer_;

  // Weak pointer for this class |ChromeCameraAppUIDelegate|, used to run on
  // main thread (mojo thread).
  base::WeakPtrFactory<ChromeCameraAppUIDelegate> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_CAMERA_APP_CHROME_CAMERA_APP_UI_DELEGATE_H_
