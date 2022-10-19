// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_data_source.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"

namespace ash {

namespace {

constexpr char kDefaultMime[] = "text/html";

void ReadFile(const base::FilePath& path,
              content::URLDataSource::GotDataCallback callback) {
  if (!base::PathIsReadable(path)) {
    std::move(callback).Run(nullptr);
    return;
  }

  // Ignore requests to directories.
  if (base::DirectoryExists(path)) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::string content;
  bool result = base::ReadFileToString(path, &content);
  DCHECK(result) << path;

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(content)));
}

}  // namespace

SystemExtensionsDataSource::SystemExtensionsDataSource(
    Profile* profile,
    const SystemExtensionId& system_extension_id,
    const GURL& system_extension_base_url)
    : profile_(profile),
      system_extension_id_(system_extension_id),
      system_extension_base_url_(system_extension_base_url) {}

SystemExtensionsDataSource::~SystemExtensionsDataSource() = default;

std::string SystemExtensionsDataSource::GetSource() {
  return system_extension_base_url_.spec();
}

#if !BUILDFLAG(OPTIMIZE_WEBUI)
bool SystemExtensionsDataSource::AllowCaching() {
  return false;
}
#endif

void SystemExtensionsDataSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  // Skip first '/' in path.
  std::string relative_path = url.path().substr(1);
  base::FilePath path =
      GetDirectoryForSystemExtension(*profile_, system_extension_id_)
          .Append(relative_path);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, path, std::move(callback)));
}

std::string SystemExtensionsDataSource::GetMimeType(const GURL& url) {
  std::string mime_type(kDefaultMime);
  std::string ext = base::FilePath(url.path_piece()).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

bool SystemExtensionsDataSource::ShouldServeMimeTypeAsContentTypeHeader() {
  return true;
}

const ui::TemplateReplacements* SystemExtensionsDataSource::GetReplacements() {
  return nullptr;
}

std::string SystemExtensionsDataSource::GetContentSecurityPolicy(
    network::mojom::CSPDirectiveName directive) {
  // System extensions are unable to create trusted types policies and require
  // disabling trusted types to run.
  if (directive == network::mojom::CSPDirectiveName::RequireTrustedTypesFor ||
      directive == network::mojom::CSPDirectiveName::TrustedTypes) {
    return std::string();
  }
  return content::URLDataSource::GetContentSecurityPolicy(directive);
}

}  // namespace ash
