// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1057510): Move TerminalSource to chromeos/components

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SOURCE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/template_expressions.h"

class Profile;

// Provides the web (html / js / css) content for crostini Terminal and crosh.
// This content is provided by chromiumos in the rootfs at
// /usr/share/chromeos-assets/crosh_builtin.
class TerminalSource : public content::URLDataSource {
 public:
  static std::unique_ptr<TerminalSource> ForCrosh(Profile* profile);

  static std::unique_ptr<TerminalSource> ForTerminal(Profile* profile);

  ~TerminalSource() override;

 private:
  TerminalSource(Profile* profile,
                 std::string source,
                 std::string default_file);

  // content::URLDataSource:
  std::string GetSource() override;
#if !BUILDFLAG(OPTIMIZE_WEBUI)
  bool AllowCaching() override;
#endif
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  const ui::TemplateReplacements* GetReplacements() override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;

  Profile* profile_;
  std::string source_;
  std::string default_file_;
  ui::TemplateReplacements replacements_;

  DISALLOW_COPY_AND_ASSIGN(TerminalSource);
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_TERMINAL_SOURCE_H_
