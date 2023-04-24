// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_DATA_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_DATA_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"

class Profile;

namespace ash {

class SystemExtensionsDataSource : public content::URLDataSource {
 public:
  SystemExtensionsDataSource(Profile* profile,
                             const SystemExtensionId& system_extension_id,
                             const GURL& system_extension_base_url);
  ~SystemExtensionsDataSource() override;

 private:
  std::string GetSource() override;

#if !BUILDFLAG(OPTIMIZE_WEBUI)
  bool AllowCaching() override;
#endif

  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;

  std::string GetMimeType(const GURL& url) override;

  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  const ui::TemplateReplacements* GetReplacements() override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

  raw_ptr<Profile, ExperimentalAsh> profile_;
  const SystemExtensionId system_extension_id_;
  const GURL system_extension_base_url_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_DATA_SOURCE_H_
