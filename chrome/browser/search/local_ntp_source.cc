// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_ntp_source.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/local_ntp_js_integrity.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/promos/promo_data.h"
#include "chrome/browser/search/promos/promo_service.h"
#include "chrome/browser/search/promos/promo_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "chrome/browser/search/search_suggest/search_suggest_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/search/omnibox_mojo_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/local_ntp_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/search_provider_logos/logo_common.h"
#include "components/search_provider_logos/logo_observer.h"
#include "components/search_provider_logos/logo_service.h"
#include "components/search_provider_logos/switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/gurl.h"

using search_provider_logos::EncodedLogo;
using search_provider_logos::EncodedLogoCallback;
using search_provider_logos::LogoCallbacks;
using search_provider_logos::LogoCallbackReason;
using search_provider_logos::LogoMetadata;
using search_provider_logos::LogoService;

namespace {

// Language code used to check features run in English in the US.
const char kEnUSLanguageCode[] = "en-US";

// Signifies a locally constructed resource, i.e. not from grit/.
const int kLocalResource = -1;

const char kConfigDataFilename[] = "config.js";
const char kDoodleScriptFilename[] = "doodle.js";
const char kGoogleUrl[] = "https://www.google.com/";
const char kMainHtmlFilename[] = "local-ntp.html";
const char kNtpBackgroundCollectionScriptFilename[] =
    "ntp-background-collections.js";
const char kNtpBackgroundImageScriptFilename[] = "ntp-background-images.js";
const char kOneGoogleBarScriptFilename[] = "one-google.js";
const char kPromoScriptFilename[] = "promo.js";
const char kSearchSuggestionsScriptFilename[] = "search-suggestions.js";
const char kSha256[] = "sha256-";
const char kThemeCSSFilename[] = "theme.css";

const struct Resource{
  const char* filename;
  int identifier;
  const char* mime_type;
} kResources[] = {
    {"animations.css", IDR_LOCAL_NTP_ANIMATIONS_CSS, "text/css"},
    {"animations.js", IDR_LOCAL_NTP_ANIMATIONS_JS, "application/javascript"},
    {"assert.js", IDR_WEBUI_JS_ASSERT, "application/javascript"},
    {"local-ntp-common.css", IDR_LOCAL_NTP_COMMON_CSS, "text/css"},
    {"customize.css", IDR_LOCAL_NTP_CUSTOMIZE_CSS, "text/css"},
    {"customize.js", IDR_LOCAL_NTP_CUSTOMIZE_JS, "application/javascript"},
    {"doodles.css", IDR_LOCAL_NTP_DOODLES_CSS, "text/css"},
    {"doodles.js", IDR_LOCAL_NTP_DOODLES_JS, "application/javascript"},
    {"images/ntp_default_favicon.png", IDR_NTP_DEFAULT_FAVICON, "image/png"},
    {"local-ntp.css", IDR_LOCAL_NTP_CSS, "text/css"},
    {"local-ntp.js", IDR_LOCAL_NTP_JS, "application/javascript"},
    {"utils.js", IDR_LOCAL_NTP_UTILS_JS, "application/javascript"},
    {"voice.css", IDR_LOCAL_NTP_VOICE_CSS, "text/css"},
    {"voice.js", IDR_LOCAL_NTP_VOICE_JS, "application/javascript"},
    {kConfigDataFilename, kLocalResource, "application/javascript"},
    {kDoodleScriptFilename, kLocalResource, "text/javascript"},
    {kMainHtmlFilename, kLocalResource, "text/html"},
    {kNtpBackgroundCollectionScriptFilename, kLocalResource, "text/javascript"},
    {kNtpBackgroundImageScriptFilename, kLocalResource, "text/javascript"},
    {kOneGoogleBarScriptFilename, kLocalResource, "text/javascript"},
    {kPromoScriptFilename, kLocalResource, "text/javascript"},
    {kSearchSuggestionsScriptFilename, kLocalResource, "text/javascript"},
    {kThemeCSSFilename, kLocalResource, "text/css"},
    // Image may not be a jpeg but the .jpg extension here still works for other
    // filetypes. Special handling for different extensions isn't worth the
    // added complexity.
    {chrome::kChromeSearchLocalNtpBackgroundFilename, kLocalResource,
     "image/jpg"},
    {omnibox::kGoogleGIconResourceName, IDR_WEBUI_IMAGES_200_LOGO_GOOGLE_G,
     "image/png"},
    {omnibox::kBookmarkIconResourceName, IDR_LOCAL_NTP_ICONS_BOOKMARK,
     "image/svg+xml"},
    {omnibox::kCalculatorIconResourceName, IDR_LOCAL_NTP_ICONS_CALCULATOR,
     "image/svg+xml"},
    {omnibox::kClockIconResourceName, IDR_LOCAL_NTP_ICONS_CLOCK,
     "image/svg+xml"},
    {omnibox::kDriveDocsIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_DOCS,
     "image/svg+xml"},
    {omnibox::kDriveFolderIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_FOLDER,
     "image/svg+xml"},
    {omnibox::kDriveFormIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_FORM,
     "image/svg+xml"},
    {omnibox::kDriveImageIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_IMAGE,
     "image/svg+xml"},
    {omnibox::kDriveLogoIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_LOGO,
     "image/svg+xml"},
    {omnibox::kDrivePdfIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_PDF,
     "image/svg+xml"},
    {omnibox::kDriveSheetsIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_SHEETS,
     "image/svg+xml"},
    {omnibox::kDriveSlidesIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_SLIDES,
     "image/svg+xml"},
    {omnibox::kDriveVideoIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_VIDEO,
     "image/svg+xml"},
    {omnibox::kExtensionAppIconResourceName, IDR_LOCAL_NTP_ICONS_EXTENSION_APP,
     "image/svg+xml"},
    {omnibox::kPageIconResourceName, IDR_LOCAL_NTP_ICONS_PAGE, "image/svg+xml"},
    {omnibox::kSearchIconResourceName, IDR_WEBUI_IMAGES_ICON_SEARCH,
     "image/svg+xml"},
    {omnibox::kTrendingUpIconResourceName, IDR_LOCAL_NTP_ICONS_TRENDING_UP,
     "image/svg+xml"},
};

// This enum must match the numbering for NTPSearchSuggestionsRequestStatusi in
// enums.xml. Do not reorder or remove items, and update kMaxValue when new
// items are added.
enum class SearchSuggestionsRequestStatus {
  UNKNOWN_ERROR = 0,
  RECEIVED_RESPONSE = 1,
  SIGNED_OUT = 2,
  OPTED_OUT = 3,
  IMPRESSION_CAP = 4,
  FROZEN = 5,
  FATAL_ERROR = 6,

  kMaxValue = FATAL_ERROR
};

// Strips any query parameters from the specified path.
std::string StripParameters(const std::string& path) {
  return path.substr(0, path.find("?"));
}

// Adds a localized string keyed by resource id to the dictionary.
void AddString(base::DictionaryValue* dictionary,
               const std::string& key,
               int resource_id) {
  dictionary->SetString(key, l10n_util::GetStringUTF16(resource_id));
}

// Populates |translated_strings| dictionary for the local NTP. |is_google|
// indicates that this page is the Google local NTP.
std::unique_ptr<base::DictionaryValue> GetTranslatedStrings(bool is_google) {
  auto translated_strings = std::make_unique<base::DictionaryValue>();

  AddString(translated_strings.get(), "thumbnailRemovedNotification",
            IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED);
  AddString(translated_strings.get(), "removeThumbnailTooltip",
            IDS_NEW_TAB_REMOVE_THUMBNAIL_TOOLTIP);
  AddString(translated_strings.get(), "undoThumbnailRemove",
            IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  AddString(translated_strings.get(), "restoreThumbnailsShort",
            IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK);
  AddString(translated_strings.get(), "attributionIntro",
            IDS_NEW_TAB_ATTRIBUTION_INTRO);
  AddString(translated_strings.get(), "title", IDS_NEW_TAB_TITLE);
  AddString(translated_strings.get(), "mostVisitedTitle",
            IDS_NEW_TAB_MOST_VISITED);

  if (is_google) {
    AddString(translated_strings.get(), "searchboxPlaceholder",
              IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD);

    // Custom Backgrounds
    AddString(translated_strings.get(), "defaultWallpapers",
              IDS_NTP_CUSTOM_BG_CHROME_WALLPAPERS);
    AddString(translated_strings.get(), "uploadImage",
              IDS_NTP_CUSTOM_BG_UPLOAD_AN_IMAGE);
    AddString(translated_strings.get(), "restoreDefaultBackground",
              IDS_NTP_CUSTOM_BG_RESTORE_DEFAULT);
    AddString(translated_strings.get(), "selectChromeWallpaper",
              IDS_NTP_CUSTOM_BG_SELECT_A_COLLECTION);
    AddString(translated_strings.get(), "selectionDone",
              IDS_NTP_CUSTOM_LINKS_DONE);
    AddString(translated_strings.get(), "selectionCancel",
              IDS_NTP_CUSTOM_BG_CANCEL);
    AddString(translated_strings.get(), "connectionErrorNoPeriod",
              IDS_NTP_CONNECTION_ERROR_NO_PERIOD);
    AddString(translated_strings.get(), "connectionError",
              IDS_NTP_CONNECTION_ERROR);
    AddString(translated_strings.get(), "moreInfo", IDS_NTP_ERROR_MORE_INFO);
    AddString(translated_strings.get(), "backgroundsUnavailable",
              IDS_NTP_CUSTOM_BG_BACKGROUNDS_UNAVAILABLE);
    AddString(translated_strings.get(), "customizeThisPage",
              IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL);
    AddString(translated_strings.get(), "backLabel",
              IDS_NTP_CUSTOM_BG_BACK_LABEL);
    AddString(translated_strings.get(), "selectedLabel",
              IDS_NTP_CUSTOM_BG_IMAGE_SELECTED);

    // Custom Links
    AddString(translated_strings.get(), "addLinkTitle",
              IDS_NTP_CUSTOM_LINKS_ADD_SHORTCUT_TITLE);
    AddString(translated_strings.get(), "addLinkTooltip",
              IDS_NTP_CUSTOM_LINKS_ADD_SHORTCUT_TOOLTIP);
    AddString(translated_strings.get(), "editLinkTitle",
              IDS_NTP_CUSTOM_LINKS_EDIT_SHORTCUT);
    AddString(translated_strings.get(), "editLinkTooltip",
              IDS_NTP_CUSTOM_LINKS_EDIT_SHORTCUT_TOOLTIP);
    AddString(translated_strings.get(), "nameField", IDS_NTP_CUSTOM_LINKS_NAME);
    AddString(translated_strings.get(), "urlField", IDS_NTP_CUSTOM_LINKS_URL);
    AddString(translated_strings.get(), "linkRemove",
              IDS_NTP_CUSTOM_LINKS_REMOVE);
    AddString(translated_strings.get(), "linkCancel",
              IDS_NTP_CUSTOM_LINKS_CANCEL);
    AddString(translated_strings.get(), "linkDone", IDS_NTP_CUSTOM_LINKS_DONE);
    AddString(translated_strings.get(), "invalidUrl",
              IDS_NTP_CUSTOM_LINKS_INVALID_URL);
    AddString(translated_strings.get(), "linkRemovedMsg",
              IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED);
    AddString(translated_strings.get(), "linkEditedMsg",
              IDS_NTP_CONFIRM_MSG_SHORTCUT_EDITED);
    AddString(translated_strings.get(), "linkAddedMsg",
              IDS_NTP_CONFIRM_MSG_SHORTCUT_ADDED);
    AddString(translated_strings.get(), "restoreDefaultLinks",
              IDS_NTP_CONFIRM_MSG_RESTORE_DEFAULTS);
    AddString(translated_strings.get(), "linkCantCreate",
              IDS_NTP_CUSTOM_LINKS_CANT_CREATE);
    AddString(translated_strings.get(), "linkCantEdit",
              IDS_NTP_CUSTOM_LINKS_CANT_EDIT);
    AddString(translated_strings.get(), "linkCantRemove",
              IDS_NTP_CUSTOM_LINKS_CANT_REMOVE);

    // Doodle Sharing
    AddString(translated_strings.get(), "shareDoodle",
              IDS_NTP_DOODLE_SHARE_LABEL);
    AddString(translated_strings.get(), "shareClose",
              IDS_NTP_DOODLE_SHARE_DIALOG_CLOSE_LABEL);
    AddString(translated_strings.get(), "shareFacebook",
              IDS_NTP_DOODLE_SHARE_DIALOG_FACEBOOK_LABEL);
    AddString(translated_strings.get(), "shareTwitter",
              IDS_NTP_DOODLE_SHARE_DIALOG_TWITTER_LABEL);
    AddString(translated_strings.get(), "shareMail",
              IDS_NTP_DOODLE_SHARE_DIALOG_MAIL_LABEL);
    AddString(translated_strings.get(), "copyLink",
              IDS_NTP_DOODLE_SHARE_DIALOG_COPY_LABEL);
    AddString(translated_strings.get(), "shareLink",
              IDS_NTP_DOODLE_SHARE_DIALOG_LINK_LABEL);

    // Voice Search
    AddString(translated_strings.get(), "audioError",
              IDS_NEW_TAB_VOICE_AUDIO_ERROR);
    AddString(translated_strings.get(), "details", IDS_NEW_TAB_VOICE_DETAILS);
    AddString(translated_strings.get(), "fakeboxMicrophoneTooltip",
              IDS_TOOLTIP_MIC_SEARCH);
    AddString(translated_strings.get(), "languageError",
              IDS_NEW_TAB_VOICE_LANGUAGE_ERROR);
    AddString(translated_strings.get(), "learnMore", IDS_LEARN_MORE);
    AddString(translated_strings.get(), "listening",
              IDS_NEW_TAB_VOICE_LISTENING);
    AddString(translated_strings.get(), "networkError",
              IDS_NEW_TAB_VOICE_NETWORK_ERROR);
    AddString(translated_strings.get(), "noTranslation",
              IDS_NEW_TAB_VOICE_NO_TRANSLATION);
    AddString(translated_strings.get(), "noVoice", IDS_NEW_TAB_VOICE_NO_VOICE);
    AddString(translated_strings.get(), "permissionError",
              IDS_NEW_TAB_VOICE_PERMISSION_ERROR);
    AddString(translated_strings.get(), "ready", IDS_NEW_TAB_VOICE_READY);
    AddString(translated_strings.get(), "tryAgain",
              IDS_NEW_TAB_VOICE_TRY_AGAIN);
    AddString(translated_strings.get(), "waiting", IDS_NEW_TAB_VOICE_WAITING);
    AddString(translated_strings.get(), "otherError",
              IDS_NEW_TAB_VOICE_OTHER_ERROR);
    AddString(translated_strings.get(), "voiceCloseTooltip",
              IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP);
    AddString(translated_strings.get(), "voiceSearchClosed",
              IDS_NEW_TAB_VOICE_SEARCH_CLOSED);

    // Realbox
    AddString(translated_strings.get(), "realboxSeparator",
              IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR);
    AddString(translated_strings.get(), "removeSuggestion",
              IDS_OMNIBOX_REMOVE_SUGGESTION);
    AddString(translated_strings.get(), "hideSuggestions",
              IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON);
    AddString(translated_strings.get(), "showSuggestions",
              IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON);
    AddString(translated_strings.get(), "hideSection",
              IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON);
    AddString(translated_strings.get(), "showSection",
              IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON);

    // Promos
    AddString(translated_strings.get(), "dismissPromo", IDS_NTP_DISMISS_PROMO);
  }

  return translated_strings;
}

std::string GetThemeCSS(Profile* profile) {
  SkColor background_color =
      ThemeService::GetThemeProviderForProfile(profile)
          .GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);

  // Required to prevent the default background color from flashing before the
  // page is initialized (the body, which contains theme color, is hidden until
  // initialization finishes). Removed after initialization.
  return base::StringPrintf(
      "html:not(.inited) { background-color: #%02X%02X%02X; }",
      SkColorGetR(background_color), SkColorGetG(background_color),
      SkColorGetB(background_color));
}

std::string ReadBackgroundImageData(const base::FilePath& profile_path) {
  std::string data_string;
  base::ReadFileToString(
      profile_path.AppendASCII(chrome::kChromeSearchLocalNtpBackgroundFilename),
      &data_string);
  return data_string;
}

void ServeBackgroundImageData(content::URLDataSource::GotDataCallback callback,
                              std::string data_string) {
  std::move(callback).Run(base::RefCountedString::TakeString(&data_string));
}

std::string GetLocalNtpPath() {
  return std::string(chrome::kChromeSearchScheme) + "://" +
         std::string(chrome::kChromeSearchLocalNtpHost) + "/";
}

base::Value ConvertCollectionInfoToDict(
    const std::vector<CollectionInfo>& collection_info) {
  base::Value collections(base::Value::Type::LIST);
  for (const CollectionInfo& collection : collection_info) {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("collectionId", base::Value(collection.collection_id));
    dict.SetKey("collectionName", base::Value(collection.collection_name));
    dict.SetKey("previewImageUrl",
                base::Value(collection.preview_image_url.spec()));
    collections.Append(std::move(dict));
  }
  return collections;
}

base::Value ConvertCollectionImageToDict(
    const std::vector<CollectionImage>& collection_image) {
  base::Value images(base::Value::Type::LIST);
  for (const CollectionImage& image : collection_image) {
    base::Value dict(base::Value::Type::DICTIONARY);
    dict.SetKey("thumbnailImageUrl",
                base::Value(image.thumbnail_image_url.spec()));
    dict.SetKey("imageUrl", base::Value(image.image_url.spec()));
    dict.SetKey("collectionId", base::Value(image.collection_id));
    base::Value attributions(base::Value::Type::LIST);
    for (const auto& attribution : image.attribution) {
      attributions.Append(base::Value(attribution));
    }
    dict.SetKey("attributions", std::move(attributions));
    dict.SetKey("attributionActionUrl",
                base::Value(image.attribution_action_url.spec()));
    images.Append(std::move(dict));
  }
  return images;
}

scoped_refptr<base::RefCountedString> GetOGBString(
    const base::Optional<OneGoogleBarData>& og) {
  base::DictionaryValue dict;
  if (og.has_value()) {
    dict.SetString("barHtml", og->bar_html);
    dict.SetString("inHeadScript", og->in_head_script);
    dict.SetString("inHeadStyle", og->in_head_style);
    dict.SetString("afterBarScript", og->after_bar_script);
    dict.SetString("endOfBodyHtml", og->end_of_body_html);
    dict.SetString("endOfBodyScript", og->end_of_body_script);
  } else {
    dict.SetString("barHtml", std::string());
  }

  std::string js;
  base::JSONWriter::Write(dict, &js);
  js = "var og = " + js + ";";
  return scoped_refptr<base::RefCountedString>(
      base::RefCountedString::TakeString(&js));
}

scoped_refptr<base::RefCountedString> GetPromoString(
    const base::Optional<PromoData>& promo) {
  base::DictionaryValue dict;
  if (promo.has_value()) {
    dict.SetString("promoHtml", promo->promo_html);
    dict.SetString("promoLogUrl", promo->promo_log_url.spec());
    dict.SetString("promoId", promo->promo_id);
    dict.SetBoolean("canOpenExtensionsPage", promo->can_open_extensions_page);
  }

  std::string js;
  base::JSONWriter::Write(dict, &js);
  js = "var promo = " + js + ";";
  return scoped_refptr<base::RefCountedString>(
      base::RefCountedString::TakeString(&js));
}

std::unique_ptr<base::DictionaryValue> ConvertSearchSuggestDataToDict(
    const base::Optional<SearchSuggestData>& data) {
  auto result = std::make_unique<base::DictionaryValue>();
  if (data.has_value()) {
    result->SetString("suggestionsHtml", data->suggestions_html);
    result->SetString("suggestionsEndOfBodyScript", data->end_of_body_script);
  } else {
    result->SetString("suggestionsHtml", std::string());
  }
  return result;
}

std::string ConvertLogoImageToBase64(
    scoped_refptr<base::RefCountedString> encoded_image,
    std::string mime_type) {
  if (!encoded_image)
    return std::string();

  std::string base64;
  base::Base64Encode(encoded_image->data(), &base64);
  return base::StringPrintf("data:%s;base64,%s", mime_type.c_str(),
                            base64.c_str());
}

std::string LogoTypeToString(search_provider_logos::LogoType type) {
  switch (type) {
    case search_provider_logos::LogoType::SIMPLE:
      return "SIMPLE";
    case search_provider_logos::LogoType::ANIMATED:
      return "ANIMATED";
    case search_provider_logos::LogoType::INTERACTIVE:
      return "INTERACTIVE";
  }
  NOTREACHED();
  return std::string();
}

std::unique_ptr<base::DictionaryValue> ConvertLogoMetadataToDict(
    const LogoMetadata& meta) {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetString("type", LogoTypeToString(meta.type));
  result->SetString("onClickUrl", meta.on_click_url.spec());
  result->SetString("altText", meta.alt_text);
  result->SetString("mimeType", meta.mime_type);
  result->SetString("darkMimeType", meta.dark_mime_type);
  result->SetString("animatedUrl", meta.animated_url.spec());
  result->SetString("darkAnimatedUrl", meta.dark_animated_url.spec());
  result->SetInteger("iframeWidthPx", meta.iframe_width_px);
  result->SetInteger("iframeHeightPx", meta.iframe_height_px);
  result->SetString("logUrl", meta.log_url.spec());
  result->SetString("ctaLogUrl", meta.cta_log_url.spec());
  result->SetString("shortLink", meta.short_link.spec());
  result->SetString("darkBackgroundColor", meta.dark_background_color);

  if (meta.share_button_x >= 0 && meta.share_button_y >= 0 &&
      !meta.share_button_icon.empty() && !meta.share_button_bg.empty()) {
    result->SetInteger("shareButtonX", meta.share_button_x);
    result->SetInteger("shareButtonY", meta.share_button_y);
    result->SetDouble("shareButtonOpacity", meta.share_button_opacity);
    result->SetString("shareButtonIcon", meta.share_button_icon);
    result->SetString("shareButtonBg", meta.share_button_bg);
  }

  if (meta.dark_share_button_x >= 0 && meta.dark_share_button_y >= 0 &&
      !meta.dark_share_button_icon.empty() &&
      !meta.dark_share_button_bg.empty()) {
    result->SetInteger("darkShareButtonX", meta.dark_share_button_x);
    result->SetInteger("darkShareButtonY", meta.dark_share_button_y);
    result->SetDouble("darkShareButtonOpacity", meta.dark_share_button_opacity);
    result->SetString("darkShareButtonIcon", meta.dark_share_button_icon);
    result->SetString("darkShareButtonBg", meta.dark_share_button_bg);
  }

  GURL full_page_url = meta.full_page_url;
  result->SetString("fullPageUrl", full_page_url.spec());

  // The fpdoodle url is always relative to google.com, for testing it needs to
  // be replaced with the demo url provided on the command line via
  // --google-base-url.
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();
  std::string url = full_page_url.spec();
  auto pos = url.find(kGoogleUrl);
  if (google_base_url.is_valid() && pos != std::string::npos) {
    url.replace(pos, strlen(kGoogleUrl), google_base_url.spec());
    result->SetString("fullPageUrl", url);
  }

  return result;
}

std::string GetErrorDict(const ErrorInfo& error) {
  base::DictionaryValue error_info;
  error_info.SetBoolean("net_error", error.error_type == ErrorType::NET_ERROR);
  error_info.SetBoolean("service_error",
                        error.error_type == ErrorType::SERVICE_ERROR);
  error_info.SetInteger("net_error_no", error.net_error);

  std::string js_text;
  JSONStringValueSerializer serializer(&js_text);
  serializer.Serialize(error_info);

  return js_text;
}

// Return the URL that the custom background should be loaded from.
// Either chrome-search://local-ntp/background.jpg or a valid, secure URL.
GURL GetCustomBackgroundURL(PrefService* pref_service) {
  if (pref_service->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice))
    return GURL(chrome::kChromeSearchLocalNtpBackgroundUrl);

  const base::DictionaryValue* background_info =
      pref_service->GetDictionary(prefs::kNtpCustomBackgroundDict);
  if (!background_info)
    return GURL();

  const base::Value* background_url =
      background_info->FindKey("background_url");
  if (!background_url)
    return GURL();

  GURL url(background_url->GetString());
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme))
    return GURL();

  return url;
}

}  // namespace

// Keeps the search engine configuration data to be included on the Local NTP,
// and will also keep track of any changes in search engine provider to
// recompute this data.
class LocalNtpSource::SearchConfigurationProvider
    : public TemplateURLServiceObserver {
 public:
  explicit SearchConfigurationProvider(TemplateURLService* service)
      : service_(service) {
    DCHECK(service_);
    service_->AddObserver(this);
    UpdateConfigData();
  }

  bool DefaultSearchProviderIsGoogle() {
    return search::DefaultSearchProviderIsGoogle(service_);
  }

  ~SearchConfigurationProvider() override {
    if (service_)
      service_->RemoveObserver(this);
  }

  const std::string& config_data_js() const { return config_data_js_; }
  const std::string& config_data_integrity() const {
    return config_data_integrity_;
  }

 private:
  // Updates the configuration data for the local NTP.
  void UpdateConfigData() {
    bool is_google = search::DefaultSearchProviderIsGoogle(service_);
    const GURL google_base_url =
        GURL(service_->search_terms_data().GoogleBaseURLValue());
    base::DictionaryValue config_data;
    config_data.Set("translatedStrings", GetTranslatedStrings(is_google));
    config_data.SetBoolean("isGooglePage", is_google);
    config_data.SetString("googleBaseUrl", google_base_url.spec());
    config_data.SetBoolean("isAccessibleBrowser",
                           content::BrowserAccessibilityState::GetInstance()
                               ->IsAccessibleBrowser());

    if (is_google) {
      config_data.SetBoolean("richerPicker", true);
      config_data.SetBoolean("realboxEnabled",
                             ntp_features::IsRealboxEnabled());
      config_data.SetBoolean("realboxMatchOmniboxTheme",
                             base::FeatureList::IsEnabled(
                                 ntp_features::kRealboxMatchOmniboxTheme));
      config_data.SetBoolean(
          "useGoogleGIcon",
          base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon));
    }

    // Serialize the dictionary.
    std::string js_text;
    JSONStringValueSerializer serializer(&js_text);
    serializer.Serialize(config_data);

    config_data_js_ = "var configData = ";
    config_data_js_.append(js_text);
    config_data_js_.append(";");

    std::string config_sha256 = crypto::SHA256HashString(config_data_js_);
    base::Base64Encode(config_sha256, &config_data_integrity_);
  }

  void OnTemplateURLServiceChanged() override {
    // The search provider may have changed, keep the config data valid.
    UpdateConfigData();
  }

  void OnTemplateURLServiceShuttingDown() override {
    service_->RemoveObserver(this);
    service_ = nullptr;
  }

  TemplateURLService* service_;

  std::string config_data_js_;
  std::string config_data_integrity_;
};

class LocalNtpSource::DesktopLogoObserver {
 public:
  DesktopLogoObserver() {}

  // Get the cached logo.
  void GetCachedLogo(LogoService* service,
                     content::URLDataSource::GotDataCallback callback) {
    StartGetLogo(service, std::move(callback), /*from_cache=*/true);
  }

  // Get the fresh logo corresponding to a previous request for a cached logo.
  // If that previous request is still ongoing, then schedule the callback to be
  // called when the fresh logo comes in. If it's not, then start a new request
  // and schedule the cached logo to be handed back.
  //
  // Strictly speaking, it's not a "fresh" logo anymore, but it should be the
  // same logo that would have been fresh relative to the corresponding cached
  // request, or perhaps one newer.
  void GetFreshLogo(LogoService* service,
                    int requested_version,
                    content::URLDataSource::GotDataCallback callback) {
    bool from_cache = (requested_version <= version_finished_);
    StartGetLogo(service, std::move(callback), from_cache);
  }

 private:
  void OnLogoAvailable(content::URLDataSource::GotDataCallback callback,
                       LogoCallbackReason type,
                       const base::Optional<EncodedLogo>& logo) {
    scoped_refptr<base::RefCountedString> response;
    auto ddl = std::make_unique<base::DictionaryValue>();
    ddl->SetInteger("v", version_started_);
    if (type == LogoCallbackReason::DETERMINED) {
      ddl->SetBoolean("usable", true);
      if (logo.has_value()) {
        ddl->SetString("image",
                       ConvertLogoImageToBase64(logo->encoded_image,
                                                logo->metadata.mime_type));
        ddl->SetString("dark_image",
                       ConvertLogoImageToBase64(logo->dark_encoded_image,
                                                logo->metadata.dark_mime_type));
        ddl->Set("metadata", ConvertLogoMetadataToDict(logo->metadata));
      } else {
        ddl->SetKey("image", base::Value());
        ddl->SetKey("dark_image", base::Value());
        ddl->SetKey("metadata", base::Value());
      }
    } else {
      ddl->SetBoolean("usable", false);
    }

    std::string js;
    base::JSONWriter::Write(*ddl, &js);
    js = "var ddl = " + js + ";";
    response = base::RefCountedString::TakeString(&js);
    std::move(callback).Run(response);
  }

  void OnCachedLogoAvailable(content::URLDataSource::GotDataCallback callback,
                             LogoCallbackReason type,
                             const base::Optional<EncodedLogo>& logo) {
    OnLogoAvailable(std::move(callback), type, logo);
  }

  void OnFreshLogoAvailable(content::URLDataSource::GotDataCallback callback,
                            LogoCallbackReason type,
                            const base::Optional<EncodedLogo>& logo) {
    OnLogoAvailable(std::move(callback), type, logo);
    OnRequestCompleted(type, logo);
  }

  void OnRequestCompleted(LogoCallbackReason type,
                          const base::Optional<EncodedLogo>& logo) {
    version_finished_ = version_started_;
  }

  void StartGetLogo(LogoService* service,
                    content::URLDataSource::GotDataCallback callback,
                    bool from_cache) {
    EncodedLogoCallback cached, fresh;
    LogoCallbacks callbacks;
    if (from_cache) {
      callbacks.on_cached_encoded_logo_available =
          base::BindOnce(&DesktopLogoObserver::OnCachedLogoAvailable,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback));
      callbacks.on_fresh_encoded_logo_available =
          base::BindOnce(&DesktopLogoObserver::OnRequestCompleted,
                         weak_ptr_factory_.GetWeakPtr());
    } else {
      callbacks.on_fresh_encoded_logo_available =
          base::BindOnce(&DesktopLogoObserver::OnFreshLogoAvailable,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback));
    }
    if (!observing()) {
      ++version_started_;
    }
    service->GetLogo(std::move(callbacks), /*for_webui_ntp=*/false);
  }

  bool observing() const {
    DCHECK_LE(version_finished_, version_started_);
    DCHECK_LE(version_started_, version_finished_ + 1);
    return version_started_ != version_finished_;
  }

  int version_started_ = 0;
  int version_finished_ = 0;

  base::WeakPtrFactory<DesktopLogoObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopLogoObserver);
};

LocalNtpSource::LocalNtpSource(Profile* profile)
    : profile_(profile),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile_)),
      one_google_bar_service_(
          OneGoogleBarServiceFactory::GetForProfile(profile_)),
      promo_service_(PromoServiceFactory::GetForProfile(profile_)),
      search_suggest_service_(
          SearchSuggestServiceFactory::GetForProfile(profile_)),
      logo_service_(nullptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |ntp_background_service_| is null in incognito, or when the feature is
  // disabled.
  if (ntp_background_service_)
    ntp_background_service_observer_.Add(ntp_background_service_);

  // |one_google_bar_service_| is null in incognito, or when the feature is
  // disabled.
  if (one_google_bar_service_)
    one_google_bar_service_observer_.Add(one_google_bar_service_);

  // |search_suggest_service_| is null in incognito, or when the feature is
  // disabled.
  if (search_suggest_service_)
    search_suggest_service_observer_.Add(search_suggest_service_);

  // |promo_service_| is null in incognito, or when the feature is
  // disabled.
  if (promo_service_)
    promo_service_observer_.Add(promo_service_);

  logo_service_ = LogoServiceFactory::GetForProfile(profile_);
  logo_observer_ = std::make_unique<DesktopLogoObserver>();

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service) {
    search_config_provider_ =
        std::make_unique<SearchConfigurationProvider>(template_url_service);
  }
}

LocalNtpSource::~LocalNtpSource() = default;

std::string LocalNtpSource::GetSource() {
  return chrome::kChromeSearchLocalNtpHost;
}

void LocalNtpSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug/1009127): Simplify usages of |path| since |url| is available.
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  std::string stripped_path = StripParameters(path);
  if (stripped_path == kConfigDataFilename) {
    std::string config_data_js = search_config_provider_->config_data_js();
    std::move(callback).Run(
        base::RefCountedString::TakeString(&config_data_js));
    return;
  }
  if (stripped_path == kThemeCSSFilename) {
    std::string theme_css = GetThemeCSS(profile_);
    std::move(callback).Run(base::RefCountedString::TakeString(&theme_css));
    return;
  }

  if (stripped_path == chrome::kChromeSearchLocalNtpBackgroundFilename) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&ReadBackgroundImageData, profile_->GetPath()),
        base::BindOnce(&ServeBackgroundImageData, std::move(callback)));
    return;
  }

  if (stripped_path == kNtpBackgroundCollectionScriptFilename) {
    if (!ntp_background_service_) {
      std::move(callback).Run(nullptr);
      return;
    }
    ntp_background_collections_requests_.emplace_back(base::TimeTicks::Now(),
                                                      std::move(callback));
    ntp_background_service_->FetchCollectionInfo();
    return;
  }

  if (stripped_path == kNtpBackgroundImageScriptFilename) {
    if (!ntp_background_service_) {
      std::move(callback).Run(nullptr);
      return;
    }
    std::string collection_id_param;
    GURL path_url = GURL(chrome::kChromeSearchLocalNtpUrl).Resolve(path);
    if (net::GetValueForKeyInQuery(path_url, "collection_id",
                                   &collection_id_param)) {
      ntp_background_image_info_requests_.emplace_back(base::TimeTicks::Now(),
                                                       std::move(callback));
      ntp_background_service_->FetchCollectionImageInfo(collection_id_param);
    } else {
      std::move(callback).Run(nullptr);
    }
    return;
  }

  if (stripped_path == kOneGoogleBarScriptFilename) {
    if (!one_google_bar_service_) {
      std::move(callback).Run(nullptr);
    } else {
      ServeOneGoogleBarWhenAvailable(std::move(callback));
    }
    return;
  }

  if (stripped_path == kPromoScriptFilename) {
    if (!promo_service_) {
      std::move(callback).Run(nullptr);
    } else {
      ServePromoWhenAvailable(std::move(callback));
    }
    return;
  }

  // Search suggestions always used a cached value, so there is no need to
  // refresh the data until the old data is used.
  if (stripped_path == kSearchSuggestionsScriptFilename) {
    if (!search_suggest_service_) {
      std::move(callback).Run(nullptr);
      return;
    }

    // Currently Vasco search suggestions are only available for en-US
    // users. If this restriction is expanded or removed in the future this
    // check must be changed.
    if (one_google_bar_service_->language_code() != kEnUSLanguageCode) {
      std::string no_suggestions =
          "var searchSuggestions = {suggestionsHtml: ''}";
      std::move(callback).Run(
          base::RefCountedString::TakeString(&no_suggestions));
      return;
    }

    ServeSearchSuggestionsIfAvailable(std::move(callback));

    pending_search_suggest_request_ = base::TimeTicks::Now();
    search_suggest_service_->Refresh();
    return;
  }

  if (stripped_path == kDoodleScriptFilename) {
    if (!logo_service_) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::string version_string;
    int version = 0;
    GURL url = GURL(chrome::kChromeSearchLocalNtpUrl).Resolve(path);
    if (net::GetValueForKeyInQuery(url, "v", &version_string) &&
        base::StringToInt(version_string, &version)) {
      logo_observer_->GetFreshLogo(logo_service_, version, std::move(callback));
    } else {
      logo_observer_->GetCachedLogo(logo_service_, std::move(callback));
    }
    return;
  }

  if (stripped_path == kMainHtmlFilename) {
    if (search_config_provider_->DefaultSearchProviderIsGoogle()) {
      InitiatePromoAndOGBRequests();
    }

    std::string force_doodle_param;
    GURL path_url = GURL(chrome::kChromeSearchLocalNtpUrl).Resolve(path);
    if (net::GetValueForKeyInQuery(path_url, "force-doodle",
                                   &force_doodle_param)) {
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      command_line->AppendSwitchASCII(
          search_provider_logos::switches::kGoogleDoodleUrl,
          "https://www.gstatic.com/chrome/ntp/doodle_test/ddljson_desktop" +
              force_doodle_param + ".json");
    }

    // TODO(dbeam): rewrite this class to WebUIDataSource instead of
    // URLDataSource, and get magical $i18n{} replacement for free.
    ui::TemplateReplacements replacements;

    const std::string& app_locale = g_browser_process->GetApplicationLocale();
    webui::SetLoadTimeDataDefaults(app_locale, &replacements);

    replacements["animationsIntegrity"] =
        base::StrCat({kSha256, ANIMATIONS_JS_INTEGRITY});
    replacements["assertIntegrity"] =
        base::StrCat({kSha256, ASSERT_JS_INTEGRITY});
    replacements["configDataIntegrity"] = base::StrCat(
        {kSha256, search_config_provider_->config_data_integrity()});
    replacements["localNtpCustomizeIntegrity"] =
        base::StrCat({kSha256, CUSTOMIZE_JS_INTEGRITY});
    replacements["doodlesIntegrity"] =
        base::StrCat({kSha256, DOODLES_JS_INTEGRITY});
    replacements["localNtpIntegrity"] =
        base::StrCat({kSha256, LOCAL_NTP_JS_INTEGRITY});
    replacements["utilsIntegrity"] =
        base::StrCat({kSha256, UTILS_JS_INTEGRITY});
    replacements["localNtpVoiceIntegrity"] =
        base::StrCat({kSha256, VOICE_JS_INTEGRITY});
    // TODO(dbeam): why is this needed? How does it interact with
    // URLDataSource::GetContentSecurityPolicy*() methods?
    replacements["contentSecurityPolicy"] = GetContentSecurityPolicyForNTP();

    replacements["customizeMenu"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL);
    replacements["customizeButton"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_BUTTON_LABEL);
    replacements["cancelButton"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOM_BG_CANCEL);
    replacements["doneButton"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOM_LINKS_DONE);
    replacements["backgroundsOption"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MENU_BACKGROUND_LABEL);
    replacements["customBackgroundDisabled"] = l10n_util::GetStringUTF8(
        IDS_NTP_CUSTOMIZE_MENU_BACKGROUND_DISABLED_LABEL);
    replacements["shortcutsOption"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL);
    replacements["colorsOption"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MENU_COLOR_LABEL);
    replacements["uploadImage"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_UPLOAD_FROM_DEVICE_LABEL);
    replacements["noBackground"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_NO_BACKGROUND_LABEL);
    replacements["myShortcuts"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL);
    replacements["shortcutsCurated"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC);
    replacements["mostVisited"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL);
    replacements["shortcutsSuggested"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC);
    replacements["hideShortcuts"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_LABEL);
    replacements["hideShortcutsDesc"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_DESC);
    replacements["installedThemeDesc"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_3PT_THEME_DESC);
    replacements["uninstallButton"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL);
    replacements["backLabel"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOM_BG_BACK_LABEL);
    replacements["refreshDaily"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOM_BG_DAILY_REFRESH);
    replacements["colorPickerLabel"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL);
    replacements["defaultThemeLabel"] =
        l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_DEFAULT_LABEL);

    replacements["bgPreloader"] = "";
    GURL custom_background_url = GetCustomBackgroundURL(profile_->GetPrefs());
    if (custom_background_url.is_valid()) {
      replacements["bgPreloader"] = "<link rel=\"preload\" href=\"" +
                                    custom_background_url.spec() +
                                    "\" as=\"image\">";
    }

    bool realbox_enabled = ntp_features::IsRealboxEnabled();
    replacements["hiddenIfRealboxEnabled"] = realbox_enabled ? "hidden" : "";
    replacements["hiddenIfRealboxDisabled"] = realbox_enabled ? "" : "hidden";

    bool use_google_g_icon =
        base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon);
    replacements["realboxDefaultIcon"] = use_google_g_icon
                                             ? omnibox::kGoogleGIconResourceName
                                             : omnibox::kSearchIconResourceName;

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::string html_string = bundle.LoadDataResourceString(IDR_LOCAL_NTP_HTML);
    base::StringPiece html(html_string);
    std::string replaced = ui::ReplaceTemplateExpressions(html, replacements);
    std::move(callback).Run(base::RefCountedString::TakeString(&replaced));
    return;
  }

  float scale = 1.0f;
  std::string filename;
  webui::ParsePathAndScale(
      GURL(GetLocalNtpPath() + stripped_path), &filename, &scale);
  ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactor(scale);

  for (size_t i = 0; i < base::size(kResources); ++i) {
    if (filename == kResources[i].filename) {
      scoped_refptr<base::RefCountedMemory> response(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
              kResources[i].identifier, scale_factor));
      std::move(callback).Run(response.get());
      return;
    }
  }
  std::move(callback).Run(nullptr);
}

std::string LocalNtpSource::GetMimeType(const std::string& path) {
  const std::string stripped_path = StripParameters(path);
  for (size_t i = 0; i < base::size(kResources); ++i) {
    if (stripped_path == kResources[i].filename)
      return kResources[i].mime_type;
  }
  return std::string();
}

bool LocalNtpSource::AllowCaching() {
  // Some resources served by LocalNtpSource, i.e. config.js, are dynamically
  // generated and could differ on each access. To avoid using old cached
  // content on reload, disallow caching here. Otherwise, it fails to reflect
  // newly revised user configurations in the page.
  return false;
}

bool LocalNtpSource::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  DCHECK(url.host_piece() == chrome::kChromeSearchLocalNtpHost);
  if (!InstantService::ShouldServiceRequest(url, browser_context,
                                            render_process_id)) {
    return false;
  }

  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    std::string filename;
    webui::ParsePathAndScale(url, &filename, nullptr);
    for (size_t i = 0; i < base::size(kResources); ++i) {
      if (filename == kResources[i].filename)
        return true;
    }
  }
  return false;
}

bool LocalNtpSource::ShouldAddContentSecurityPolicy() {
  // The Content Security Policy is served as a meta tag in local NTP html.
  // We disable the HTTP Header version here to avoid a conflicting policy. See
  // GetContentSecurityPolicy.
  return false;
}

std::string LocalNtpSource::GetContentSecurityPolicyForNTP() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL google_base_url = google_util::CommandLineGoogleBaseURL();

  // Allow embedding of the most visited iframe, as well as the account
  // switcher and the notifications dropdown from the One Google Bar, and/or
  // the iframe for interactive Doodles.
  std::string child_src_csp = base::StringPrintf(
      "child-src %s https://*.google.com/ %s;",
      chrome::kChromeSearchMostVisitedUrl, google_base_url.spec().c_str());

  // Restrict scripts in the main page to those listed here. However,
  // 'strict-dynamic' allows those scripts to load dependencies not listed here.
  std::string script_src_csp = base::StringPrintf(
      "script-src 'strict-dynamic' 'sha256-%s' 'sha256-%s' 'sha256-%s' "
      "'sha256-%s' 'sha256-%s' 'sha256-%s' 'sha256-%s' 'sha256-%s';",
      ANIMATIONS_JS_INTEGRITY, ASSERT_JS_INTEGRITY, CUSTOMIZE_JS_INTEGRITY,
      DOODLES_JS_INTEGRITY, LOCAL_NTP_JS_INTEGRITY, UTILS_JS_INTEGRITY,
      VOICE_JS_INTEGRITY,
      search_config_provider_->config_data_integrity().c_str());

  return GetContentSecurityPolicy(network::mojom::CSPDirectiveName::ObjectSrc) +
         GetContentSecurityPolicy(network::mojom::CSPDirectiveName::StyleSrc) +
         GetContentSecurityPolicy(network::mojom::CSPDirectiveName::ImgSrc) +
         child_src_csp + script_src_csp;
}

void LocalNtpSource::OnCollectionInfoAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ntp_background_collections_requests_.empty())
    return;

  std::string js_errors =
      "var collErrors = " +
      GetErrorDict(ntp_background_service_->collection_error_info());

  scoped_refptr<base::RefCountedString> result;
  std::string js;
  base::JSONWriter::Write(
      ConvertCollectionInfoToDict(ntp_background_service_->collection_info()),
      &js);
  js = "var coll = " + js + "; " + js_errors;
  result = base::RefCountedString::TakeString(&js);

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& request : ntp_background_collections_requests_) {
    std::move(request.callback).Run(result);
    base::TimeDelta delta = now - request.start_time;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency", delta);
    // Any response where no collections are returned is considered a failure.
    if (ntp_background_service_->collection_info().empty()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.BackgroundService.Collections.RequestLatency.Failure",
          delta);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.BackgroundService.Collections.RequestLatency.Success",
          delta);
    }
  }
  ntp_background_collections_requests_.clear();
}

void LocalNtpSource::OnCollectionImagesAvailable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (ntp_background_image_info_requests_.empty())
    return;

  std::string js_errors =
      "var collImgErrors = " +
      GetErrorDict(ntp_background_service_->collection_images_error_info());

  scoped_refptr<base::RefCountedString> result;
  std::string js;
  base::JSONWriter::Write(ConvertCollectionImageToDict(
                              ntp_background_service_->collection_images()),
                          &js);
  js = "var collImg = " + js + "; " + js_errors;
  result = base::RefCountedString::TakeString(&js);

  base::TimeTicks now = base::TimeTicks::Now();
  for (auto& request : ntp_background_image_info_requests_) {
    std::move(request.callback).Run(result);
    base::TimeDelta delta = now - request.start_time;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency", delta);
    // Any response where no images are returned is considered a failure.
    if (ntp_background_service_->collection_images().empty()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.BackgroundService.Images.RequestLatency.Failure", delta);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.BackgroundService.Images.RequestLatency.Success", delta);
    }
  }
  ntp_background_image_info_requests_.clear();
}

void LocalNtpSource::OnNtpBackgroundServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ntp_background_service_observer_.RemoveAll();
  ntp_background_service_ = nullptr;
}

void LocalNtpSource::OnOneGoogleBarDataUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ServeOneGoogleBar(one_google_bar_service_->one_google_bar_data());
}

void LocalNtpSource::OnOneGoogleBarServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  one_google_bar_service_observer_.RemoveAll();
  one_google_bar_service_ = nullptr;
}

void LocalNtpSource::OnPromoDataUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ServePromo(promo_service_->promo_data());
}

void LocalNtpSource::OnPromoServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  promo_service_observer_.RemoveAll();
  promo_service_ = nullptr;
}

void LocalNtpSource::OnSearchSuggestDataUpdated() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!pending_search_suggest_request_.has_value()) {
    return;
  }

  SearchSuggestLoader::Status result =
      search_suggest_service_->search_suggest_status();
  base::TimeDelta delta =
      base::TimeTicks::Now() - *pending_search_suggest_request_;
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.SearchSuggestions.RequestLatencyV2",
                             delta);
  SearchSuggestionsRequestStatus request_status =
      SearchSuggestionsRequestStatus::UNKNOWN_ERROR;

  if (result == SearchSuggestLoader::Status::SIGNED_OUT) {
    request_status = SearchSuggestionsRequestStatus::SIGNED_OUT;
  } else if (result == SearchSuggestLoader::Status::OPTED_OUT) {
    request_status = SearchSuggestionsRequestStatus::OPTED_OUT;
  } else if (result == SearchSuggestLoader::Status::IMPRESSION_CAP) {
    request_status = SearchSuggestionsRequestStatus::IMPRESSION_CAP;
  } else if (result == SearchSuggestLoader::Status::REQUESTS_FROZEN) {
    request_status = SearchSuggestionsRequestStatus::FROZEN;
  } else if (result == SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS) {
    request_status = SearchSuggestionsRequestStatus::RECEIVED_RESPONSE;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.SearchSuggestions.RequestLatencyV2."
        "SuccessWithSuggestions",
        delta);
  } else if (result == SearchSuggestLoader::Status::OK_WITHOUT_SUGGESTIONS) {
    request_status = SearchSuggestionsRequestStatus::RECEIVED_RESPONSE;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.SearchSuggestions.RequestLatencyV2."
        "SuccessWithoutSuggestions",
        delta);
  } else if (result == SearchSuggestLoader::Status::FATAL_ERROR) {
    request_status = SearchSuggestionsRequestStatus::FATAL_ERROR;
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.SearchSuggestions.RequestLatencyV2.Failure", delta);
  }
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.SearchSuggestions.RequestStatusV2",
                            request_status);

  pending_search_suggest_request_ = base::nullopt;
}

void LocalNtpSource::OnSearchSuggestServiceShuttingDown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  search_suggest_service_observer_.RemoveAll();
  search_suggest_service_ = nullptr;
}

void LocalNtpSource::ServeSearchSuggestionsIfAvailable(
    content::URLDataSource::GotDataCallback callback) {
  base::Optional<SearchSuggestData> data =
      search_suggest_service_->search_suggest_data();

  if (search_suggest_service_->search_suggest_status() ==
      SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS) {
    search_suggest_service_->SuggestionsDisplayed();
  }
  scoped_refptr<base::RefCountedString> result;
  std::string js;
  base::JSONWriter::Write(*ConvertSearchSuggestDataToDict(data), &js);
  js = "var searchSuggestions  = " + js + ";";
  result = base::RefCountedString::TakeString(&js);
  std::move(callback).Run(result);
}

void LocalNtpSource::ServeOneGoogleBar(
    const base::Optional<OneGoogleBarData>& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!pending_one_google_bar_request_.has_value()) {
    return;
  }

  scoped_refptr<base::RefCountedString> result;
  if (data.has_value()) {
    result = GetOGBString(data);
  }

  base::TimeDelta delta =
      base::TimeTicks::Now() - *pending_one_google_bar_request_;
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency", delta);
  if (result) {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency.Success",
                               delta);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency.Failure",
                               delta);
  }
  for (auto& callback : one_google_bar_callbacks_) {
    std::move(callback).Run(result);
  }
  pending_one_google_bar_request_ = base::nullopt;
  one_google_bar_callbacks_.clear();
}

void LocalNtpSource::ServeOneGoogleBarWhenAvailable(
    content::URLDataSource::GotDataCallback callback) {
  base::Optional<OneGoogleBarData> data =
      one_google_bar_service_->one_google_bar_data();

  if (!pending_one_google_bar_request_.has_value()) {
    std::move(callback).Run(GetOGBString(data));
  } else {
    one_google_bar_callbacks_.push_back(std::move(callback));
  }
}

void LocalNtpSource::ServePromo(const base::Optional<PromoData>& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!pending_promo_request_.has_value()) {
    return;
  }

  scoped_refptr<base::RefCountedString> result = GetPromoString(data);

  base::TimeDelta delta = base::TimeTicks::Now() - *pending_promo_request_;
  UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency", delta);
  if (promo_service_->promo_status() == PromoService::Status::OK_WITH_PROMO) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", delta);
  } else if (promo_service_->promo_status() ==
             PromoService::Status::OK_BUT_BLOCKED) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", delta);
  } else if (promo_service_->promo_status() ==
             PromoService::Status::OK_WITHOUT_PROMO) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", delta);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency2.Failure",
                               delta);
  }
  for (auto& callback : promo_callbacks_) {
    std::move(callback).Run(result);
  }
  pending_promo_request_ = base::nullopt;
  promo_callbacks_.clear();
}

void LocalNtpSource::ServePromoWhenAvailable(
    content::URLDataSource::GotDataCallback callback) {
  base::Optional<PromoData> data = promo_service_->promo_data();

  if (!pending_promo_request_.has_value()) {
    std::move(callback).Run(GetPromoString(data));
  } else {
    promo_callbacks_.push_back(std::move(callback));
  }
}

void LocalNtpSource::InitiatePromoAndOGBRequests() {
  if (one_google_bar_service_) {
    pending_one_google_bar_request_ = base::TimeTicks::Now();
    one_google_bar_service_->Refresh();
  }
  if (promo_service_) {
    pending_promo_request_ = base::TimeTicks::Now();
    promo_service_->Refresh();
  }
}

LocalNtpSource::NtpBackgroundRequest::NtpBackgroundRequest(
    base::TimeTicks start_time,
    content::URLDataSource::GotDataCallback callback)
    : start_time(start_time), callback(std::move(callback)) {}

LocalNtpSource::NtpBackgroundRequest::NtpBackgroundRequest(
    NtpBackgroundRequest&&) = default;
LocalNtpSource::NtpBackgroundRequest& LocalNtpSource::NtpBackgroundRequest::
operator=(NtpBackgroundRequest&&) = default;

LocalNtpSource::NtpBackgroundRequest::~NtpBackgroundRequest() = default;
