// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/terminal_source.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ash/file_system_provider/provider_interface.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/webui_allowlist.h"

namespace {
constexpr base::FilePath::CharType kTerminalRoot[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/crosh_builtin");
constexpr char kDefaultMime[] = "text/html";

class TerminalFileSystemProvider
    : public ash::file_system_provider::ExtensionProvider {
 public:
  TerminalFileSystemProvider()
      : ash::file_system_provider::ExtensionProvider(
            ProfileManager::GetPrimaryUserProfile(),
            ash::file_system_provider::ProviderId::CreateFromExtensionId(
                guest_os::kTerminalSystemAppId),
            ash::file_system_provider::Capabilities{
                .configurable = true,
                .watchable = false,
                .multiple_mounts = true,
                .source = extensions::FileSystemProviderSource::SOURCE_NETWORK},
            l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME),
            /*icon_set=*/std::nullopt) {}
  bool RequestMount(
      Profile* profile,
      ash::file_system_provider::RequestMountCallback callback) override {
    guest_os::LaunchTerminalHome(profile, display::kInvalidDisplayId,
                                 /*restore_id=*/0);
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::File::FILE_OK));
    return true;
  }
};

// Attempts to read |path| as plain file.  If read fails, attempts to read
// |path|.gz and decompress. Returns true if either file is read ok.
bool ReadUncompressedOrGzip(base::FilePath path, std::string* content) {
  bool result = base::ReadFileToString(path, content);
  if (!result) {
    result =
        base::ReadFileToString(base::FilePath(path.value() + ".gz"), content);
    result = compression::GzipUncompress(*content, content);
  }
  return result;
}

void ReadFile(const base::FilePath downloads,
              const std::string& relative_path,
              content::URLDataSource::GotDataCallback callback) {
  base::FilePath path;
  std::string content;
  bool result = false;

  // If chrome://flags#terminal-dev set on dev channel, check Downloads.
  if (chrome::GetChannel() <= version_info::Channel::DEV &&
      base::FeatureList::IsEnabled(ash::features::kTerminalDev)) {
    path = downloads.Append("crosh_builtin").Append(relative_path);
    result = ReadUncompressedOrGzip(path, &content);
  }
  if (!result) {
    path = base::FilePath(kTerminalRoot).Append(relative_path);
    result = ReadUncompressedOrGzip(path, &content);
  }

  // Terminal gets files from /usr/share/chromeos-assets/crosh-builtin.
  // In chromium tests, these files don't exist, so we serve dummy values.
  if (!result) {
    static const base::NoDestructor<base::flat_map<std::string, std::string>>
        kTestFiles({
            {"html/crosh.html", ""},
            {"html/terminal.html", "<script src='/js/terminal.js'></script>"},
            {"js/terminal.js",
             "chrome.terminalPrivate.openVmshellProcess([], () => {})"},
        });
    auto it = kTestFiles->find(relative_path);
    if (it != kTestFiles->end()) {
      content = it->second;
      result = true;
    }
  }

  DCHECK(result) << path;
  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(content)));
}
}  // namespace

// static
std::unique_ptr<TerminalSource> TerminalSource::ForCrosh(Profile* profile) {
  return base::WrapUnique(
      new TerminalSource(profile, chrome::kChromeUIUntrustedCroshURL, false));
}

// static
std::unique_ptr<TerminalSource> TerminalSource::ForTerminal(Profile* profile) {
  ash::file_system_provider::Service::Get(profile)->RegisterProvider(
      std::make_unique<TerminalFileSystemProvider>());
  return base::WrapUnique(new TerminalSource(
      profile, chrome::kChromeUIUntrustedTerminalURL,
      profile->GetPrefs()
          ->FindPreference(crostini::prefs::kTerminalSshAllowedByPolicy)
          ->GetValue()
          ->GetBool()));
}

TerminalSource::TerminalSource(Profile* profile,
                               std::string source,
                               bool ssh_allowed)
    : profile_(profile),
      source_(source),
      ssh_allowed_(ssh_allowed),
      downloads_(file_manager::util::GetDownloadsFolderForProfile(profile)) {
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(profile);
  const url::Origin terminal_origin = url::Origin::Create(GURL(source));
  CHECK(!terminal_origin.opaque());
  for (auto permission :
       {ContentSettingsType::CLIPBOARD_READ_WRITE, ContentSettingsType::COOKIES,
        ContentSettingsType::IMAGES, ContentSettingsType::JAVASCRIPT,
        ContentSettingsType::NOTIFICATIONS, ContentSettingsType::POPUPS,
        ContentSettingsType::SOUND}) {
    webui_allowlist->RegisterAutoGrantedPermission(terminal_origin, permission);
  }
  webui_allowlist->RegisterAutoGrantedThirdPartyCookies(
      terminal_origin, {ContentSettingsPattern::Wildcard()});
}

TerminalSource::~TerminalSource() = default;

std::string TerminalSource::GetSource() {
  return source_;
}

void TerminalSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // skip first '/' in path.
  std::string path = url.path().substr(1);
  if (path.empty()) {
    path = "html/terminal.html";
  }

  // Refresh the $i8n{themeColor} replacement for css files.
  if (base::EndsWith(path, ".css", base::CompareCase::INSENSITIVE_ASCII)) {
    GURL contents_url;
    std::optional<SkColor> opener_background_color;
    content::WebContents* contents = wc_getter.Run();
    if (contents) {
      contents_url = contents->GetVisibleURL();
      TabStripModel* tab_strip;
      int tab_index;
      extensions::ExtensionTabUtil::GetTabStripModel(contents, &tab_strip,
                                                     &tab_index);
      tabs::TabModel* opener_tab = tab_strip->GetOpenerOfTabAt(tab_index);
      if (opener_tab) {
        CHECK(opener_tab->contents());
        opener_background_color = opener_tab->contents()->GetBackgroundColor();
      }
    }
    replacements_["themeColor"] =
        base::EscapeForHTML(guest_os::GetTerminalSettingBackgroundColor(
            profile_, contents_url, opener_background_color));
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, downloads_, path, std::move(callback)));
}

std::string TerminalSource::GetMimeType(const GURL& url) {
  std::string mime_type(kDefaultMime);
  std::string ext = base::FilePath(url.path_piece()).Extension();
  if (!ext.empty()) {
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  }
  return mime_type;
}

bool TerminalSource::ShouldServeMimeTypeAsContentTypeHeader() {
  // TerminalSource pages include js modules which require an explicit MimeType.
  return true;
}

const ui::TemplateReplacements* TerminalSource::GetReplacements() {
  return &replacements_;
}

std::string TerminalSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  // CSP required for SSH.
  if (ssh_allowed_) {
    switch (directive) {
      case network::mojom::CSPDirectiveName::ConnectSrc:
        return "connect-src *;";
      case network::mojom::CSPDirectiveName::FrameAncestors:
        return "frame-ancestors 'self';";
      case network::mojom::CSPDirectiveName::FrameSrc:
        return "frame-src 'self';";
      case network::mojom::CSPDirectiveName::ObjectSrc:
        return "object-src 'self';";
      case network::mojom::CSPDirectiveName::ScriptSrc:
        return "script-src 'self' 'wasm-unsafe-eval';";
      case network::mojom::CSPDirectiveName::WorkerSrc:
        return "worker-src 'self';";
      default:
        break;
    }
  }

  switch (directive) {
    case network::mojom::CSPDirectiveName::ImgSrc:
      return "img-src * data: blob:;";
    case network::mojom::CSPDirectiveName::MediaSrc:
      return "media-src data:;";
    case network::mojom::CSPDirectiveName::StyleSrc:
      return "style-src * 'unsafe-inline'; font-src *;";
    case network::mojom::CSPDirectiveName::RequireTrustedTypesFor:
      [[fallthrough]];
    case network::mojom::CSPDirectiveName::TrustedTypes:
      // TODO(crbug.com/40137141): Trusted Type remaining WebUI
      // This removes require-trusted-types-for and trusted-types directives
      // from the CSP header.
      return std::string();
    default:
      return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
}

// Improve security, and it is required for wasm SharedArrayBuffer.
std::string TerminalSource::GetCrossOriginOpenerPolicy() {
  return "same-origin";
}

// Required for wasm SharedArrayBuffer.
std::string TerminalSource::GetCrossOriginEmbedderPolicy() {
  return "require-corp";
}
