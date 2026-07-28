// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_CLIPBOARD_CONTEXT_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_CLIPBOARD_CONTEXT_H_

#include "components/enterprise/data_controls/core/browser/clipboard_context.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/content_browser_client.h"
#include "ui/base/clipboard/clipboard_metadata.h"

namespace data_controls {

// Clank/desktop implementation of `data_controls::ClipboardContext`.
class ChromeClipboardContext : public ClipboardContext {
 public:
  ChromeClipboardContext(content::ClipboardEndpoint source,
                         content::ClipboardEndpoint destination,
                         ui::ClipboardMetadata metadata);
  ChromeClipboardContext(content::ClipboardEndpoint source,
                         ui::ClipboardMetadata metadata);
  ~ChromeClipboardContext();

  // Converts `source` into a `CopiedTextSource`. `CopiedTextSource::context` is
  // always populated, but `CopiedTextSource::url` may be left empty depending
  // on the policies that are set and broader clipboard copy context.
  //
  // This function should only be used to obtain a clipboard source for paste
  // reports and scans.
  static enterprise_connectors::ContentMetaData::CopiedTextSource
  GetClipboardSource(const content::ClipboardEndpoint& source,
                     const content::ClipboardEndpoint& destination,
                     const char* scope_pref);

  // ClipboardContext:
  GURL source_url() const override;
  GURL destination_url() const override;
  enterprise_connectors::ContentMetaData::CopiedTextSource
  data_controls_copied_text_source() const override;
  ui::ClipboardFormatType format_type() const override;
  std::optional<size_t> size() const override;
  std::string source_active_user() const override;
  std::string destination_active_user() const override;

 private:
  content::ClipboardEndpoint source_;
  content::ClipboardEndpoint destination_;
  ui::ClipboardMetadata metadata_;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_CLIPBOARD_CONTEXT_H_
