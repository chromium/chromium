// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_ssl_host_state_delegate.h"

#include "base/functional/callback.h"
#include "net/base/hash_value.h"

using content::SSLHostStateDelegate;

namespace android_webview {

namespace internal {

CertPolicy::CertPolicy() {
}
CertPolicy::~CertPolicy() {
}

// For an allowance, we consider a given |cert| to be a match to a saved
// allowed cert if the |error| is an exact match to or subset of the errors
// in the saved CertStatus.
bool CertPolicy::Check(const net::X509Certificate& cert, int error) const {
  net::SHA256HashValue fingerprint = cert.CalculateChainFingerprint256();
  auto allowed_iter = allowed_.find(fingerprint);
  if ((allowed_iter != allowed_.end()) && (allowed_iter->second & error) &&
      ((allowed_iter->second & error) == error)) {
    return true;
  }
  return false;
}

void CertPolicy::Allow(const net::X509Certificate& cert, int error) {
  // If this same cert had already been saved with a different error status,
  // this will replace it with the new error status.
  net::SHA256HashValue fingerprint = cert.CalculateChainFingerprint256();
  allowed_[fingerprint] = error;
}

}  // namespace internal

AwSSLHostStateDelegate::AwSSLHostStateDelegate() {
}

AwSSLHostStateDelegate::~AwSSLHostStateDelegate() {
}

void AwSSLHostStateDelegate::HostRanInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  // Intentional no-op for Android WebView.
}

bool AwSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int child_id,
    InsecureContentType content_type) {
  // Intentional no-op for Android WebView.
  return false;
}

void AwSSLHostStateDelegate::AllowHttpForHost(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  // Intentional no-op for Android WebView.
}

bool AwSSLHostStateDelegate::IsHttpAllowedForHost(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  // Intentional no-op for Android WebView. Return value does not matter as
  // HTTPS-First Mode is not enabled on WebView.
  return false;
}

void AwSSLHostStateDelegate::SetHttpsEnforcementForHost(
    const std::string& host,
    bool enforce,
    content::StoragePartition* storage_partition) {
  // Intentional no-op for Android WebView.
}

bool AwSSLHostStateDelegate::IsHttpsEnforcedForUrl(
    const GURL& url,
    content::StoragePartition* storage_partition) {
  // Intentional no-op for Android WebView. Return value does not matter as
  // HTTPS-First Mode is not enabled on WebView.
  return false;
}

void AwSSLHostStateDelegate::AllowCert(
    const std::string& host,
    const net::X509Certificate& cert,
    int error,
    content::StoragePartition* storage_partition) {
  cert_policy_for_host_[host].Allow(cert, error);
}

void AwSSLHostStateDelegate::Clear(
    base::RepeatingCallback<bool(const std::string&)> host_filter) {
  if (!host_filter) {
    cert_policy_for_host_.clear();
    return;
  }

  for (auto it = cert_policy_for_host_.begin();
       it != cert_policy_for_host_.end();) {
    auto next_it = std::next(it);

    if (host_filter.Run(it->first))
      cert_policy_for_host_.erase(it);

    it = next_it;
  }
}

SSLHostStateDelegate::CertJudgment AwSSLHostStateDelegate::QueryPolicy(
    const std::string& host,
    const net::X509Certificate& cert,
    int error,
    content::StoragePartition* storage_partition) {
  auto iter = cert_policy_for_host_.find(host);
  if (iter != cert_policy_for_host_.end() && iter->second.Check(cert, error)) {
    return SSLHostStateDelegate::ALLOWED;
  }
  return SSLHostStateDelegate::DENIED;
}

void AwSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  cert_policy_for_host_.erase(host);
}

bool AwSSLHostStateDelegate::HasAllowException(
    const std::string& host,
    content::StoragePartition* storage_partition) {
  auto policy_iterator = cert_policy_for_host_.find(host);
  return policy_iterator != cert_policy_for_host_.end() &&
         policy_iterator->second.HasAllowException();
}

bool AwSSLHostStateDelegate::HasAllowExceptionForAnyHost(
    content::StoragePartition* storage_partition) {
  for (auto const& it : cert_policy_for_host_) {
    if (it.second.HasAllowException()) {
      return true;
    }
  }
  return false;
}

}  // namespace android_webview
