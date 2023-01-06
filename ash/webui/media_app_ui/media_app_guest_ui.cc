// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/media_app_ui/media_app_guest_ui.h"

#include "ash/webui/grit/ash_media_app_resources.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "ash/webui/web_applications/webui_test_prod_util.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/task/thread_pool.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"
#include "ui/file_manager/grit/file_manager_resources.h"

namespace ash {

namespace {

constexpr base::FilePath::CharType kFontsRoot[] =
    FILE_PATH_LITERAL("/usr/share/fonts");
constexpr char kFontRequestPrefix[] = "fonts/";

int g_media_app_window_count = 0;

// Helper class to populate MediaApp metrics for UMA and for Happiness Tracking
// surveys. Manages its own lifetime; tracking whether at least one MediaApp
// WebUI instance is still running.
class MediaAppMetricsHelper {
 public:
  static MediaAppUserActions actions;

  static void OnUiFirstNavigated() {
    // Record the number of other media app windows that currently exist when a
    // new one is created. Counts windows open with any supported file type, or
    // in the "zero state" (with no open file). Pick 50 as a sensible maximum
    // (additional windows will be recorded in the 51 bucket).
    constexpr int kMaxExpectedWindowCount = 50;
    UMA_HISTOGRAM_EXACT_LINEAR("Apps.MediaApp.Load.OtherOpenWindowCount",
                               g_media_app_window_count,
                               kMaxExpectedWindowCount);
    if (g_media_app_window_count++ == 0) {
      DCHECK(!instance);
      instance = new MediaAppMetricsHelper();
    }
  }

  static void OnUiDestroyedAfterNavigation() {
    if (--g_media_app_window_count == 0) {
      delete instance;
      instance = nullptr;
    }
  }

  MediaAppMetricsHelper(const MediaAppMetricsHelper&) = delete;
  MediaAppMetricsHelper& operator=(const MediaAppMetricsHelper&) = delete;

 private:
  MediaAppMetricsHelper() { base::AddActionCallback(callback_); }
  ~MediaAppMetricsHelper() { base::RemoveActionCallback(callback_); }

  static void OnAction(const std::string& user_action,
                       base::TimeTicks action_time) {
    actions.clicked_edit_image_in_photos =
        actions.clicked_edit_image_in_photos ||
        user_action == "MediaApp.Image.Tool.EditInPhotos";
    actions.clicked_edit_video_in_photos =
        actions.clicked_edit_video_in_photos ||
        user_action == "MediaApp.Video.Tool.EditInPhotos";
  }

  base::ActionCallback callback_ =
      base::BindRepeating(&MediaAppMetricsHelper::OnAction);

  static MediaAppMetricsHelper* instance;
};
MediaAppUserActions MediaAppMetricsHelper::actions = {false, false};
MediaAppMetricsHelper* MediaAppMetricsHelper::instance = nullptr;

bool IsFontRequest(const std::string& path) {
  return base::StartsWith(path, kFontRequestPrefix);
}

void FontLoaded(content::WebUIDataSource::GotDataCallback got_data_callback,
                std::unique_ptr<std::string> font_data,
                bool did_load_file) {
  if (font_data->size() && did_load_file) {
    std::move(got_data_callback)
        .Run(new base::RefCountedBytes(
            reinterpret_cast<const unsigned char*>(font_data->data()),
            font_data->size()));
  } else {
    std::move(got_data_callback).Run(nullptr);
  }
}

content::WebUIDataSource* CreateAndAddMediaAppUntrustedDataSource(
    content::WebUI* web_ui,
    MediaAppGuestUIDelegate* delegate) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kChromeUIMediaAppGuestURL);
  // Add resources from ash_media_app_resources.pak.
  source->AddResourcePath("app.html", IDR_MEDIA_APP_APP_HTML);
  source->AddResourcePath("receiver.js", IDR_MEDIA_APP_RECEIVER_JS);
  source->AddResourcePath("piex_module.js", IDR_MEDIA_APP_PIEX_MODULE_JS);

  // Add shared resources from ash_file_manager_resources.pak.
  source->AddResourcePath("piex/piex.js.wasm", IDR_IMAGE_LOADER_PIEX_WASM_JS);
  source->AddResourcePath("piex/piex.out.wasm", IDR_IMAGE_LOADER_PIEX_WASM);

  // Add resources from chromeos_media_app_bundle_resources.pak that are also
  // needed for mocks. If enable_cros_media_app = true, then these calls will
  // happen a second time with the same parameters. When false, we need these to
  // specify what routes are mocked by files in ./resources/mock/js. The loop is
  // irrelevant in that case.
  source->AddResourcePath("js/app_main.js", IDR_MEDIA_APP_APP_MAIN_JS);
  source->AddResourcePath("js/app_image_handler_module.js",
                          IDR_MEDIA_APP_APP_IMAGE_HANDLER_MODULE_JS);

  // Add all resources from chromeos_media_app_bundle_resources.pak.
  source->AddResourcePaths(base::make_span(
      kChromeosMediaAppBundleResources, kChromeosMediaAppBundleResourcesSize));

  // Note: go/bbsrc/flags.ts processes this.
  delegate->PopulateLoadTimeData(web_ui, source);
  source->UseStringsJs();

  source->AddFrameAncestor(GURL(kChromeUIMediaAppURL));
  // By default, prevent all network access.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      "default-src blob: 'self';");
  // Need to explicitly set |worker-src| because CSP falls back to |child-src|
  // which is none.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");
  // Allow images to also handle data urls.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src blob: data: 'self';");
  // Allow styles to include inline styling needed for Polymer elements.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
  // Allow loading PDFs as blob URLs.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src blob:;");
  // Required to successfully load PDFs in the `<embed>` element.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src blob:;");
  // Allow wasm.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'wasm-eval';");
  // Allow calls to Maps reverse geocoding API for loading metadata.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src 'self' https://maps.googleapis.com/maps/api/geocode/json;");

  // Allow use of SharedArrayBuffer (required by the wasm).
  source->OverrideCrossOriginOpenerPolicy("same-origin");
  source->OverrideCrossOriginEmbedderPolicy("require-corp");
  // chrome://media-app and chrome-untrusted://media-app are different origins,
  // so allow resources in the guest to be loaded cross-origin.
  source->OverrideCrossOriginResourcePolicy("cross-origin");

  // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
  source->DisableTrustedTypesCSP();
  return source;
}

}  // namespace

MediaAppGuestUI::MediaAppGuestUI(content::WebUI* web_ui,
                                 MediaAppGuestUIDelegate* delegate)
    : UntrustedWebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()) {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  content::WebUIDataSource* untrusted_source =
      CreateAndAddMediaAppUntrustedDataSource(web_ui, delegate);

  MaybeConfigureTestableDataSource(
      untrusted_source, "media_app/untrusted",
      base::BindRepeating(&IsFontRequest),
      base::BindRepeating(&MediaAppGuestUI::StartFontDataRequest,
                          weak_factory_.GetWeakPtr()));
}

MediaAppGuestUI::~MediaAppGuestUI() {
  if (app_navigation_committed_)
    MediaAppMetricsHelper::OnUiDestroyedAfterNavigation();
}

void MediaAppGuestUI::ReadyToCommitNavigation(
    content::NavigationHandle* handle) {
  // Force-enable autoplay support.
  const std::string allowed_resource = "app.html";
  if (handle->GetURL() != GURL(kChromeUIMediaAppGuestURL + allowed_resource))
    return;

  if (!app_navigation_committed_) {
    app_navigation_committed_ = true;
    MediaAppMetricsHelper::OnUiFirstNavigated();
  }

  mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
  handle->GetRenderFrameHost()->GetRemoteAssociatedInterfaces()->GetInterface(
      &client);
  client->AddAutoplayFlags(url::Origin::Create(handle->GetURL()),
                           blink::mojom::kAutoplayFlagForceAllow);
}

void MediaAppGuestUI::StartFontDataRequest(
    const std::string& request_path,
    content::WebUIDataSource::GotDataCallback got_data_callback) {
  CHECK(IsFontRequest(request_path));
  const std::string path = request_path.substr(sizeof(kFontRequestPrefix) - 1);
  const base::FilePath font_path = base::FilePath(kFontsRoot).AppendASCII(path);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::PathExists, font_path),
      base::BindOnce(&MediaAppGuestUI::StartFontDataRequestAfterPathExists,
                     weak_factory_.GetWeakPtr(), font_path,
                     std::move(got_data_callback)));
}

void MediaAppGuestUI::StartFontDataRequestAfterPathExists(
    const base::FilePath& font_path,
    content::WebUIDataSource::GotDataCallback got_data_callback,
    bool path_exists) {
  if (path_exists) {
    auto font_data = std::make_unique<std::string>();
    std::string* data = font_data.get();
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&base::ReadFileToString, font_path, data),
        base::BindOnce(&FontLoaded, std::move(got_data_callback),
                       std::move(font_data)));

  } else {
    std::move(got_data_callback).Run(nullptr);
  }
}

MediaAppUserActions GetMediaAppUserActionsForHappinessTracking() {
  return MediaAppMetricsHelper::actions;
}

}  // namespace ash
