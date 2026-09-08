// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_TYPES_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_TYPES_H_

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENTERPRISE_DATA_CONTROLS));

namespace safe_browsing {
class ReferrerChainEntry;
using ReferrerChain =
    google::protobuf::RepeatedPtrField<safe_browsing::ReferrerChainEntry>;
}  // namespace safe_browsing

namespace content {
class BrowserContext;
class ClipboardEndpoint;
}  // namespace content

namespace enterprise_data_protection {

// Holds cached values from a clipboard source endpoint so that policy checks
// can be performed asynchronously without requiring the original source tab or
// RenderFrameHost to remain alive or unnavigated.
struct BasicPasteSource {
  BasicPasteSource();
  BasicPasteSource(const BasicPasteSource&);
  BasicPasteSource& operator=(const BasicPasteSource&);
  BasicPasteSource(BasicPasteSource&&);
  BasicPasteSource& operator=(BasicPasteSource&&);
  virtual ~BasicPasteSource();

  GURL url() const;

  bool operator==(const BasicPasteSource& other) const;

  std::optional<ui::DataTransferEndpoint> data_transfer_endpoint;
  base::WeakPtr<content::BrowserContext> browser_context;
  bool gemini_in_chrome = false;
  std::string title;
  std::string page_content_type;
};

// Extends `BasicPasteSource` to also include the active user account email.
// Suitable for Enterprise Connectors reporting and safe browsing contexts.
struct FullPasteSource : public BasicPasteSource {
  FullPasteSource();
  FullPasteSource(const FullPasteSource&);
  FullPasteSource& operator=(const FullPasteSource&);
  FullPasteSource(FullPasteSource&&);
  FullPasteSource& operator=(FullPasteSource&&);
  ~FullPasteSource() override;

  bool operator==(const FullPasteSource& other) const;

  std::string active_user;
};

// Extends `FullPasteSource` to also include copy-specific analysis data
// such as the safe browsing referrer chain at the time of copy.
struct FullCopySource : public FullPasteSource {
  FullCopySource();
  FullCopySource(const FullCopySource&);
  FullCopySource& operator=(const FullCopySource&);
  FullCopySource(FullCopySource&&);
  FullCopySource& operator=(FullCopySource&&);
  ~FullCopySource() override;

  bool operator==(const FullCopySource& other) const;

  safe_browsing::ReferrerChain referrer_chain_data;
};

// Returns a basic cached snapshot of `source` without querying user identity.
BasicPasteSource CacheBasicPasteSource(
    const content::ClipboardEndpoint& source);

// Returns a full cached snapshot of `source`, including the active user email.
FullPasteSource CacheFullPasteSource(const content::ClipboardEndpoint& source);

// Returns a full cached snapshot of `source` for copy policy checks,
// capturing the referrer chain at copy time.
FullCopySource CacheFullCopySource(const content::ClipboardEndpoint& source);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_DATA_PROTECTION_CLIPBOARD_UTILS_TYPES_H_
