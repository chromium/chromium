// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_ui.h"

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/i18n/rtl.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/media/kaleidoscope/constants.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_data_provider_impl.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_metrics_recorder.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_switches.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/language/core/browser/locale_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
#include "chrome/browser/media/kaleidoscope/grit/kaleidoscope_resources.h"
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

namespace {

// Wraps the strings in JS so they can be accessed by the code. The strings are
// placed on the window object so they can always be accessed.
const char kStringWrapper[] =
    "window.KALEIDOSCOPE_STRINGS_FALLBACK = new Map(Object.entries(%s));"
    "window.KALEIDOSCOPE_STRINGS = new Map(Object.entries(%s));";

bool OnShouldHandleRequest(const std::string& path) {
  return base::EqualsCaseInsensitiveASCII(path,
                                          "resources/_locales/strings.js");
}

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)

int GetResourceForLocale(const std::string& locale) {
  static const base::NoDestructor<base::flat_map<base::StringPiece, int>>
      kLocaleMap({
          {"en", IDR_KALEIDOSCOPE_LOCALE_EN},
          {"af", IDR_KALEIDOSCOPE_LOCALE_AF},
          {"am", IDR_KALEIDOSCOPE_LOCALE_AM},
          {"ar-eg", IDR_KALEIDOSCOPE_LOCALE_AR_EG},
          {"ar-jo", IDR_KALEIDOSCOPE_LOCALE_AR_JO},
          {"ar-ma", IDR_KALEIDOSCOPE_LOCALE_AR_MA},
          {"ar-sa", IDR_KALEIDOSCOPE_LOCALE_AR_SA},
          {"ar-xb", IDR_KALEIDOSCOPE_LOCALE_AR_XB},
          {"ar", IDR_KALEIDOSCOPE_LOCALE_AR},
          {"as", IDR_KALEIDOSCOPE_LOCALE_AS},
          {"az", IDR_KALEIDOSCOPE_LOCALE_AZ},
          {"be", IDR_KALEIDOSCOPE_LOCALE_BE},
          {"bg", IDR_KALEIDOSCOPE_LOCALE_BG},
          {"bn", IDR_KALEIDOSCOPE_LOCALE_BN},
          {"bs", IDR_KALEIDOSCOPE_LOCALE_BS},
          {"ca", IDR_KALEIDOSCOPE_LOCALE_CA},
          {"cs", IDR_KALEIDOSCOPE_LOCALE_CS},
          {"cy", IDR_KALEIDOSCOPE_LOCALE_CY},
          {"da", IDR_KALEIDOSCOPE_LOCALE_DA},
          {"de-at", IDR_KALEIDOSCOPE_LOCALE_DE_AT},
          {"de-ch", IDR_KALEIDOSCOPE_LOCALE_DE_CH},
          {"de", IDR_KALEIDOSCOPE_LOCALE_DE},
          {"el", IDR_KALEIDOSCOPE_LOCALE_EL},
          {"en-au", IDR_KALEIDOSCOPE_LOCALE_EN_AU},
          {"en-ca", IDR_KALEIDOSCOPE_LOCALE_EN_CA},
          {"en-gb", IDR_KALEIDOSCOPE_LOCALE_EN_GB},
          {"en-ie", IDR_KALEIDOSCOPE_LOCALE_EN_IE},
          {"en-in", IDR_KALEIDOSCOPE_LOCALE_EN_IN},
          {"en-nz", IDR_KALEIDOSCOPE_LOCALE_EN_NZ},
          {"en-sg", IDR_KALEIDOSCOPE_LOCALE_EN_SG},
          {"en-xa", IDR_KALEIDOSCOPE_LOCALE_EN_XA},
          {"en-xc", IDR_KALEIDOSCOPE_LOCALE_EN_XC},
          {"en-za", IDR_KALEIDOSCOPE_LOCALE_EN_ZA},
          {"es-419", IDR_KALEIDOSCOPE_LOCALE_ES_419},
          {"es-ar", IDR_KALEIDOSCOPE_LOCALE_ES_AR},
          {"es-bo", IDR_KALEIDOSCOPE_LOCALE_ES_BO},
          {"es-cl", IDR_KALEIDOSCOPE_LOCALE_ES_CL},
          {"es-co", IDR_KALEIDOSCOPE_LOCALE_ES_CO},
          {"es-cr", IDR_KALEIDOSCOPE_LOCALE_ES_CR},
          {"es-do", IDR_KALEIDOSCOPE_LOCALE_ES_DO},
          {"es-ec", IDR_KALEIDOSCOPE_LOCALE_ES_EC},
          {"es-gt", IDR_KALEIDOSCOPE_LOCALE_ES_GT},
          {"es-hn", IDR_KALEIDOSCOPE_LOCALE_ES_HN},
          {"es-mx", IDR_KALEIDOSCOPE_LOCALE_ES_MX},
          {"es-ni", IDR_KALEIDOSCOPE_LOCALE_ES_NI},
          {"es-pa", IDR_KALEIDOSCOPE_LOCALE_ES_PA},
          {"es-pe", IDR_KALEIDOSCOPE_LOCALE_ES_PE},
          {"es-pr", IDR_KALEIDOSCOPE_LOCALE_ES_PR},
          {"es-py", IDR_KALEIDOSCOPE_LOCALE_ES_PY},
          {"es-sv", IDR_KALEIDOSCOPE_LOCALE_ES_SV},
          {"es-us", IDR_KALEIDOSCOPE_LOCALE_ES_US},
          {"es-uy", IDR_KALEIDOSCOPE_LOCALE_ES_UY},
          {"es-ve", IDR_KALEIDOSCOPE_LOCALE_ES_VE},
          {"es", IDR_KALEIDOSCOPE_LOCALE_ES},
          {"et", IDR_KALEIDOSCOPE_LOCALE_ET},
          {"eu", IDR_KALEIDOSCOPE_LOCALE_EU},
          {"fa", IDR_KALEIDOSCOPE_LOCALE_FA},
          {"fil", IDR_KALEIDOSCOPE_LOCALE_FIL},
          {"fi", IDR_KALEIDOSCOPE_LOCALE_FI},
          {"fr-ca", IDR_KALEIDOSCOPE_LOCALE_FR_CA},
          {"fr-ch", IDR_KALEIDOSCOPE_LOCALE_FR_CH},
          {"fr", IDR_KALEIDOSCOPE_LOCALE_FR},
          {"gl", IDR_KALEIDOSCOPE_LOCALE_GL},
          {"gsw", IDR_KALEIDOSCOPE_LOCALE_GSW},
          {"gu", IDR_KALEIDOSCOPE_LOCALE_GU},
          {"he", IDR_KALEIDOSCOPE_LOCALE_HE},
          {"hi", IDR_KALEIDOSCOPE_LOCALE_HI},
          {"hr", IDR_KALEIDOSCOPE_LOCALE_HR},
          {"hu", IDR_KALEIDOSCOPE_LOCALE_HU},
          {"hy", IDR_KALEIDOSCOPE_LOCALE_HY},
          {"id", IDR_KALEIDOSCOPE_LOCALE_ID},
          {"in", IDR_KALEIDOSCOPE_LOCALE_IN},
          {"is", IDR_KALEIDOSCOPE_LOCALE_IS},
          {"it", IDR_KALEIDOSCOPE_LOCALE_IT},
          {"iw", IDR_KALEIDOSCOPE_LOCALE_IW},
          {"ja", IDR_KALEIDOSCOPE_LOCALE_JA},
          {"ka", IDR_KALEIDOSCOPE_LOCALE_KA},
          {"kk", IDR_KALEIDOSCOPE_LOCALE_KK},
          {"km", IDR_KALEIDOSCOPE_LOCALE_KM},
          {"kn", IDR_KALEIDOSCOPE_LOCALE_KN},
          {"ko", IDR_KALEIDOSCOPE_LOCALE_KO},
          {"ky", IDR_KALEIDOSCOPE_LOCALE_KY},
          {"ln", IDR_KALEIDOSCOPE_LOCALE_LN},
          {"lo", IDR_KALEIDOSCOPE_LOCALE_LO},
          {"lt", IDR_KALEIDOSCOPE_LOCALE_LT},
          {"lv", IDR_KALEIDOSCOPE_LOCALE_LV},
          {"mk", IDR_KALEIDOSCOPE_LOCALE_MK},
          {"ml", IDR_KALEIDOSCOPE_LOCALE_ML},
          {"mn", IDR_KALEIDOSCOPE_LOCALE_MN},
          {"mo", IDR_KALEIDOSCOPE_LOCALE_MO},
          {"mr", IDR_KALEIDOSCOPE_LOCALE_MR},
          {"ms", IDR_KALEIDOSCOPE_LOCALE_MS},
          {"my", IDR_KALEIDOSCOPE_LOCALE_MY},
          {"nb", IDR_KALEIDOSCOPE_LOCALE_NB},
          {"ne", IDR_KALEIDOSCOPE_LOCALE_NE},
          {"nl", IDR_KALEIDOSCOPE_LOCALE_NL},
          {"no", IDR_KALEIDOSCOPE_LOCALE_NO},
          {"or", IDR_KALEIDOSCOPE_LOCALE_OR},
          {"pa", IDR_KALEIDOSCOPE_LOCALE_PA},
          {"pl", IDR_KALEIDOSCOPE_LOCALE_PL},
          {"pt-br", IDR_KALEIDOSCOPE_LOCALE_PT_BR},
          {"pt-pt", IDR_KALEIDOSCOPE_LOCALE_PT_PT},
          {"pt", IDR_KALEIDOSCOPE_LOCALE_PT},
          {"ro", IDR_KALEIDOSCOPE_LOCALE_RO},
          {"ru", IDR_KALEIDOSCOPE_LOCALE_RU},
          {"si", IDR_KALEIDOSCOPE_LOCALE_SI},
          {"sk", IDR_KALEIDOSCOPE_LOCALE_SK},
          {"sl", IDR_KALEIDOSCOPE_LOCALE_SL},
          {"sq", IDR_KALEIDOSCOPE_LOCALE_SQ},
          {"sr-latn", IDR_KALEIDOSCOPE_LOCALE_SR_LATN},
          {"sr", IDR_KALEIDOSCOPE_LOCALE_SR},
          {"sv", IDR_KALEIDOSCOPE_LOCALE_SV},
          {"sw", IDR_KALEIDOSCOPE_LOCALE_SW},
          {"ta", IDR_KALEIDOSCOPE_LOCALE_TA},
          {"te", IDR_KALEIDOSCOPE_LOCALE_TE},
          {"th", IDR_KALEIDOSCOPE_LOCALE_TH},
          {"tl", IDR_KALEIDOSCOPE_LOCALE_TL},
          {"tr", IDR_KALEIDOSCOPE_LOCALE_TR},
          {"uk", IDR_KALEIDOSCOPE_LOCALE_UK},
          {"ur", IDR_KALEIDOSCOPE_LOCALE_UR},
          {"uz", IDR_KALEIDOSCOPE_LOCALE_UZ},
          {"vi", IDR_KALEIDOSCOPE_LOCALE_VI},
          {"zh-cn", IDR_KALEIDOSCOPE_LOCALE_ZH_CN},
          {"zh-hk", IDR_KALEIDOSCOPE_LOCALE_ZH_HK},
          {"zh-tw", IDR_KALEIDOSCOPE_LOCALE_ZH_TW},
          {"zh", IDR_KALEIDOSCOPE_LOCALE_ZH},
          {"zu", IDR_KALEIDOSCOPE_LOCALE_ZU},
      });

  auto it = kLocaleMap->find(locale);
  if (it == kLocaleMap->end()) {
    return IDR_KALEIDOSCOPE_LOCALE_EN;
  }
  return it->second;
}

#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

std::string GetStringsForLocale(const std::string& locale) {
  std::string str;
#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  str = ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      GetResourceForLocale(locale));
#endif
  return str;
}

void OnStringsRequest(const std::string& path,
                      content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(OnShouldHandleRequest(path));

  auto str_lang = GetStringsForLocale(
      base::ToLowerASCII(base::i18n::GetConfiguredLocale()));
  auto str_lang_en = GetStringsForLocale("en");

  base::RefCountedString* ref_contents = new base::RefCountedString();
  ref_contents->data() =
      base::StringPrintf(kStringWrapper, str_lang_en.c_str(), str_lang.c_str());
  std::move(callback).Run(ref_contents);
}

}  // anonymous namespace

// We set |enable_chrome_send| to true since we need it for browser tests.
KaleidoscopeUI::KaleidoscopeUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, CreateWebUIDataSource());
  content::WebUIDataSource::Add(browser_context,
                                CreateUntrustedWebUIDataSource());
}

KaleidoscopeUI::~KaleidoscopeUI() {
  metrics_recorder_->OnExitPage();

  // Ensure that the provider is deleted before the metrics recorder, since the
  // provider has a pointer to the metrics recorder.
  provider_.reset();
  metrics_recorder_.reset();
}

// static
content::WebUIDataSource* KaleidoscopeUI::CreateWebUIDataSource() {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(kKaleidoscopeUIHost);

  // Allows us to put content in an IFrame.
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src chrome-untrusted://kaleidoscope;");
  html_source->DisableTrustedTypesCSP();

  // Add a request filter to handle strings.js
  html_source->SetRequestFilter(base::BindRepeating(OnShouldHandleRequest),
                                base::BindRepeating(OnStringsRequest));

  // Allow workers from chrome://kaleidoscope (for testing).
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src chrome://kaleidoscope;");

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  html_source->AddResourcePath("kaleidoscope.js", IDR_KALEIDOSCOPE_JS);
  html_source->AddResourcePath("messages.js", IDR_KALEIDOSCOPE_MESSAGES_JS);

  // TODO(beccahughes): Remove
  html_source->AddResourcePath("utils.js", IDR_KALEIDOSCOPE_UTILS_JS);
  html_source->AddResourcePath("content-worker.js",
                               IDR_KALEIDOSCOPE_CONTENT_WORKER_JS);

  html_source->AddResourcePath("geometry.mojom-lite.js",
                               IDR_GEOMETRY_MOJOM_LITE_JS);
  html_source->AddResourcePath("kaleidoscope.mojom-lite.js",
                               IDR_KALEIDOSCOPE_MOJOM_LITE_JS);
  html_source->AddResourcePath(
      "chrome/browser/media/feeds/media_feeds_store.mojom-lite.js",
      IDR_MEDIA_FEEDS_STORE_MOJOM_LITE_JS);
  html_source->AddResourcePath("module.js", IDR_KALEIDOSCOPE_NTP_MODULE_JS);
  html_source->AddResourcePath("content.js", IDR_KALEIDOSCOPE_CONTENT_JS);
  html_source->AddResourcePath("shared.css", IDR_KALEIDOSCOPE_SHARED_CSS);

  html_source->SetDefaultResource(IDR_KALEIDOSCOPE_HTML);
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

  return html_source;
}

content::WebUIDataSource* KaleidoscopeUI::CreateUntrustedWebUIDataSource() {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::Create(kKaleidoscopeUntrustedContentUIURL);
  untrusted_source->DisableDenyXFrameOptions();
  untrusted_source->UseStringsJs();

  // Add a request filter to handle strings.js
  untrusted_source->SetRequestFilter(base::BindRepeating(OnShouldHandleRequest),
                                     base::BindRepeating(OnStringsRequest));

  const auto backend_url =
      GetGoogleAPIBaseURL(*base::CommandLine::ForCurrentProcess());

  // Allow scripts and styles from chrome-untrusted://resources.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources 'unsafe-inline' 'self';");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src chrome-untrusted://resources 'unsafe-inline' 'self';");
  untrusted_source->DisableTrustedTypesCSP();

  // Allow workers from chrome-untrusted://kaleidoscope.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src chrome-untrusted://kaleidoscope;");

  // Allow images and videos from anywhere.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src * data:;");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc, "media-src * data: blob:;");

  // Allow access to anywhere using fetch.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc, "connect-src *;");

  // Allow YouTube videos to be embedded.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src https://www.youtube.com;");

  // Add the URL to the backend.
  untrusted_source->AddString("googleApiUrl", backend_url.spec());

#if BUILDFLAG(ENABLE_KALEIDOSCOPE)
  untrusted_source->AddResourcePath("content.css",
                                    IDR_KALEIDOSCOPE_CONTENT_CSS);
  untrusted_source->AddResourcePath("shared.css", IDR_KALEIDOSCOPE_SHARED_CSS);
  untrusted_source->AddResourcePath("content.js", IDR_KALEIDOSCOPE_CONTENT_JS);
  untrusted_source->AddResourcePath("content-worker.js",
                                    IDR_KALEIDOSCOPE_CONTENT_WORKER_JS);
  untrusted_source->AddResourcePath("messages.js",
                                    IDR_KALEIDOSCOPE_MESSAGES_JS);
  untrusted_source->AddResourcePath("toolbar.js", IDR_KALEIDOSCOPE_TOOLBAR_JS);
  untrusted_source->AddResourcePath("side_nav_container.js",
                                    IDR_KALEIDOSCOPE_SIDE_NAV_CONTAINER_JS);
  untrusted_source->AddResourcePath("shaka-player.ui.js",
                                    IDR_KALEIDOSCOPE_SHAKA_PLAYER_JS);

  untrusted_source->AddResourcePath("geometry.mojom-lite.js",
                                    IDR_GEOMETRY_MOJOM_LITE_JS);
  untrusted_source->AddResourcePath("kaleidoscope.mojom-lite.js",
                                    IDR_KALEIDOSCOPE_MOJOM_LITE_JS);
  untrusted_source->AddResourcePath(
      "chrome/browser/media/feeds/media_feeds_store.mojom-lite.js",
      IDR_MEDIA_FEEDS_STORE_MOJOM_LITE_JS);

  // Google Sans.
  untrusted_source->AddResourcePath("resources/fonts/fonts.css",
                                    IDR_GOOGLE_SANS_CSS);
  untrusted_source->AddResourcePath("resources/fonts/GoogleSans-Bold.woff2",
                                    IDR_GOOGLE_SANS_BOLD);
  untrusted_source->AddResourcePath("resources/fonts/GoogleSans-Medium.woff2",
                                    IDR_GOOGLE_SANS_MEDIUM);
  untrusted_source->AddResourcePath("resources/fonts/GoogleSans-Regular.woff2",
                                    IDR_GOOGLE_SANS_REGULAR);
  untrusted_source->AddResourcePath(
      "resources/fonts/GoogleSansDisplay-Regular.woff2",
      IDR_GOOGLE_SANS_DISPLAY_REGULAR);

  untrusted_source->AddResourcePath("content.html",
                                    IDR_KALEIDOSCOPE_CONTENT_HTML);
#endif  // BUILDFLAG(ENABLE_KALEIDOSCOPE)

  return untrusted_source;
}

void KaleidoscopeUI::BindInterface(
    mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> provider) {
  metrics_recorder_ = std::make_unique<KaleidoscopeMetricsRecorder>();
  provider_ = std::make_unique<KaleidoscopeDataProviderImpl>(
      std::move(provider), Profile::FromWebUI(web_ui()),
      metrics_recorder_.get());
}

WEB_UI_CONTROLLER_TYPE_IMPL(KaleidoscopeUI)
