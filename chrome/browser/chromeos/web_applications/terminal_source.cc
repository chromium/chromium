// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/terminal_source.h"

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crostini/crostini_terminal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/webui/webui_allowlist.h"

namespace {
constexpr base::FilePath::CharType kTerminalRoot[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/crosh_builtin");
constexpr char kDefaultMime[] = "text/html";

void ReadFile(const std::string& relative_path,
              content::URLDataSource::GotDataCallback callback) {
  std::string content;
  base::FilePath path = base::FilePath(kTerminalRoot).Append(relative_path);
  // First look for uncompressed resource, then try for gzipped file.
  bool result = base::ReadFileToString(path, &content);
  if (!result) {
    result =
        base::ReadFileToString(base::FilePath(path.value() + ".gz"), &content);
    std::string uncompressed;
    result = compression::GzipUncompress(content, &uncompressed);
    content = std::move(uncompressed);
  }

  // Terminal gets files from /usr/share/chromeos-assets/crosh-builtin.
  // In chromium tests, these files don't exist, so we serve dummy values.
  if (!result) {
    static const base::NoDestructor<base::flat_map<std::string, std::string>>
        kTestFiles({
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

  scoped_refptr<base::RefCountedString> response =
      base::RefCountedString::TakeString(&content);
  std::move(callback).Run(response.get());
}
}  // namespace

// static
std::unique_ptr<TerminalSource> TerminalSource::ForCrosh(Profile* profile) {
  return base::WrapUnique(new TerminalSource(
      profile, chrome::kChromeUIUntrustedCroshURL, "html/crosh.html"));
}

// static
std::unique_ptr<TerminalSource> TerminalSource::ForTerminal(Profile* profile) {
  return base::WrapUnique(new TerminalSource(
      profile, chrome::kChromeUIUntrustedTerminalURL, "html/terminal.html"));
}

TerminalSource::TerminalSource(Profile* profile,
                               std::string source,
                               std::string default_file)
    : profile_(profile), source_(source), default_file_(default_file) {
  auto* webui_allowlist = WebUIAllowlist::GetOrCreate(profile);
  const url::Origin terminal_origin = url::Origin::Create(GURL(source));
  CHECK(!terminal_origin.opaque());
  for (auto permission :
       {ContentSettingsType::JAVASCRIPT, ContentSettingsType::NOTIFICATIONS,
        ContentSettingsType::CLIPBOARD_READ_WRITE, ContentSettingsType::COOKIES,
        ContentSettingsType::IMAGES, ContentSettingsType::SOUND}) {
    webui_allowlist->RegisterAutoGrantedPermission(terminal_origin, permission);
  }
}

TerminalSource::~TerminalSource() = default;

std::string TerminalSource::GetSource() {
  return source_;
}

#if !BUILDFLAG(OPTIMIZE_WEBUI)
bool TerminalSource::AllowCaching() {
  return false;
}
#endif

void TerminalSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // skip first '/' in path.
  std::string path = url.path().substr(1);
  if (path.empty())
    path = default_file_;

  // Replace $i8n{themeColor} in *.html.
  if (base::EndsWith(path, ".html", base::CompareCase::INSENSITIVE_ASCII)) {
    replacements_["themeColor"] = net::EscapeForHTML(
        crostini::GetTerminalSettingBackgroundColor(profile_));
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, path, std::move(callback)));
}

std::string TerminalSource::GetMimeType(const std::string& path) {
  std::string mime_type(kDefaultMime);
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
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
  switch (directive) {
    case network::mojom::CSPDirectiveName::ImgSrc:
      return "img-src * data: blob:;";
    case network::mojom::CSPDirectiveName::MediaSrc:
      return "media-src data:;";
    case network::mojom::CSPDirectiveName::StyleSrc:
      return "style-src * 'unsafe-inline'; font-src *;";
    case network::mojom::CSPDirectiveName::RequireTrustedTypesFor:
      FALLTHROUGH;
    case network::mojom::CSPDirectiveName::TrustedTypes:
      // TODO(crbug.com/1098685): Trusted Type remaining WebUI
      // This removes require-trusted-types-for and trusted-types directives
      // from the CSP header.
      return std::string();
    default:
      return content::URLDataSource::GetContentSecurityPolicy(directive);
  }
}
