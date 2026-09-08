// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils_types.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

namespace enterprise_data_protection {

namespace {

bool ReferrerChainsEqual(const safe_browsing::ReferrerChain& a,
                         const safe_browsing::ReferrerChain& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (int i = 0; i < a.size(); ++i) {
    if (a[i].SerializeAsString() != b[i].SerializeAsString()) {
      return false;
    }
  }
  return true;
}

}  // namespace

BasicPasteSource::BasicPasteSource() = default;
BasicPasteSource::BasicPasteSource(const BasicPasteSource&) = default;
BasicPasteSource& BasicPasteSource::operator=(const BasicPasteSource&) =
    default;
BasicPasteSource::BasicPasteSource(BasicPasteSource&&) = default;
BasicPasteSource& BasicPasteSource::operator=(BasicPasteSource&&) = default;
BasicPasteSource::~BasicPasteSource() = default;

GURL BasicPasteSource::url() const {
  if (!data_transfer_endpoint || !data_transfer_endpoint->IsUrlType() ||
      !data_transfer_endpoint->GetURL()) {
    return GURL();
  }
  return *data_transfer_endpoint->GetURL();
}

bool BasicPasteSource::operator==(const BasicPasteSource& other) const {
  return data_transfer_endpoint == other.data_transfer_endpoint &&
         browser_context.get() == other.browser_context.get() &&
         gemini_in_chrome == other.gemini_in_chrome && title == other.title &&
         page_content_type == other.page_content_type;
}

FullPasteSource::FullPasteSource() = default;
FullPasteSource::FullPasteSource(const FullPasteSource&) = default;
FullPasteSource& FullPasteSource::operator=(const FullPasteSource&) = default;
FullPasteSource::FullPasteSource(FullPasteSource&&) = default;
FullPasteSource& FullPasteSource::operator=(FullPasteSource&&) = default;
FullPasteSource::~FullPasteSource() = default;

bool FullPasteSource::operator==(const FullPasteSource& other) const {
  return static_cast<const BasicPasteSource&>(*this) ==
             static_cast<const BasicPasteSource&>(other) &&
         active_user == other.active_user;
}

FullCopySource::FullCopySource() = default;
FullCopySource::FullCopySource(const FullCopySource&) = default;
FullCopySource& FullCopySource::operator=(const FullCopySource&) = default;
FullCopySource::FullCopySource(FullCopySource&&) = default;
FullCopySource& FullCopySource::operator=(FullCopySource&&) = default;
FullCopySource::~FullCopySource() = default;

bool FullCopySource::operator==(const FullCopySource& other) const {
  return static_cast<const FullPasteSource&>(*this) ==
             static_cast<const FullPasteSource&>(other) &&
         ReferrerChainsEqual(referrer_chain_data, other.referrer_chain_data);
}

BasicPasteSource CacheBasicPasteSource(
    const content::ClipboardEndpoint& source) {
  BasicPasteSource cached;
  cached.data_transfer_endpoint = source.data_transfer_endpoint();
  if (source.browser_context()) {
    cached.browser_context = source.browser_context()->GetWeakPtr();
  }
  if (auto* web_contents = source.web_contents()) {
    cached.title = base::UTF16ToUTF8(web_contents->GetTitle());
    cached.page_content_type = web_contents->GetContentsMimeType();
    cached.gemini_in_chrome =
        glic::IsGlicGuest(web_contents) || glic::IsGlicWebUI(web_contents);
  }
  return cached;
}

FullPasteSource CacheFullPasteSource(const content::ClipboardEndpoint& source) {
  FullPasteSource cached;
  static_cast<BasicPasteSource&>(cached) = CacheBasicPasteSource(source);
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  cached.active_user =
      enterprise_connectors::ContentAreaUserProvider::GetUser(source);
#endif
  return cached;
}

FullCopySource CacheFullCopySource(const content::ClipboardEndpoint& source) {
  FullCopySource cached;
  static_cast<FullPasteSource&>(cached) = CacheFullPasteSource(source);
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  if (auto* web_contents = source.web_contents()) {
    cached.referrer_chain_data =
        enterprise_connectors::GetReferrerChain(cached.url(), *web_contents);
  }
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  return cached;
}

}  // namespace enterprise_data_protection
