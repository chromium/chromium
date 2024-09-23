// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/40677452): Move TerminalSource to chromeos/components

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_TERMINAL_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_TERMINAL_SOURCE_H_

#include <string>

#include "base/memory/raw_ptr.h"
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

  TerminalSource(const TerminalSource&) = delete;
  TerminalSource& operator=(const TerminalSource&) = delete;

  ~TerminalSource() override;

 private:
  TerminalSource(Profile* profile, std::string source, bool ssh_allowed);

  // content::URLDataSource:
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      content::URLDataSource::GotDataCallback callback) override;
  std::string GetMimeType(const GURL& url) override;
  bool ShouldServeMimeTypeAsContentTypeHeader() override;
  const ui::TemplateReplacements* GetReplacements() override;
  std::string GetContentSecurityPolicy(
      network::mojom::CSPDirectiveName directive) override;
  std::string GetCrossOriginOpenerPolicy() override;
  std::string GetCrossOriginEmbedderPolicy() override;

  raw_ptr<Profile> profile_;
  std::string source_;
  std::string default_file_;
  const bool ssh_allowed_;
  const base::FilePath downloads_;
  ui::TemplateReplacements replacements_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_TERMINAL_SOURCE_H_
