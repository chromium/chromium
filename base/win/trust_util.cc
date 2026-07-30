// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/trust_util.h"

#include <windows.h>

#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_wintrust_data.h"
#include "build/branding_buildflags.h"

namespace base::win {

namespace {

constexpr wchar_t kGooglePublisherName[] = L"Google LLC";

bool ExtractSubjectNameFromWintrustData(HANDLE verify_trust_state_data,
                                        std::wstring* subject_name) {
  CRYPT_PROVIDER_DATA* prov_data =
      WTHelperProvDataFromStateData(verify_trust_state_data);
  if (!prov_data || prov_data->csSigners == 0) {
    return false;
  }

  CRYPT_PROVIDER_SGNR* signer = WTHelperGetProvSignerFromChain(
      prov_data, /*idxSigner=*/0, /*fCounterSigner=*/false,
      /*idxCounterSigner=*/0);
  if (!signer || !signer->pChainContext) {
    return false;
  }

  const CERT_CHAIN_CONTEXT* cert_chain_context = signer->pChainContext;
  if (cert_chain_context->cChain == 0 || !cert_chain_context->rgpChain[0]) {
    return false;
  }

  CERT_SIMPLE_CHAIN* simple_chain = cert_chain_context->rgpChain[0];
  if (simple_chain->cElement == 0 || !simple_chain->rgpElement[0]) {
    return false;
  }

  const CERT_CONTEXT* cert_context = simple_chain->rgpElement[0]->pCertContext;
  if (!cert_context) {
    return false;
  }

  // Get the subject. First ask how long the name is, including null terminator.
  DWORD length = CertGetNameStringW(
      cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, /*dwFlags=*/0,
      /*pvTypePara=*/nullptr, /*pszNameString=*/nullptr, /*cchNameString=*/0);
  if (length <= 1) {
    return false;
  }

  CertGetNameStringW(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, /*dwFlags=*/0,
                     /*pvTypePara=*/nullptr,
                     base::WriteInto(subject_name, length), length);
  return true;
}

std::wstring GetCertificateSubjectName(const FilePath& binary_path) {
  ScopedWintrustData wintrust_data(binary_path);
  std::wstring subject_name;
  if (wintrust_data.is_valid()) {
    ExtractSubjectNameFromWintrustData(wintrust_data.hWVTStateData(),
                                       &subject_name);
  }
  return subject_name;
}

}  // namespace

bool IsWintrustDataTrusted(const ScopedWintrustData& wintrust_data,
                           bool verify_publisher) {
  if (!wintrust_data.is_valid() || !wintrust_data.hWVTStateData()) {
    return false;
  }
  if (!verify_publisher) {
    return true;
  }

  std::wstring subject_name;
  if (!ExtractSubjectNameFromWintrustData(wintrust_data.hWVTStateData(),
                                          &subject_name)) {
    return false;
  }

  FilePath exe_path;
  std::wstring expected_publisher = kGooglePublisherName;
  if (PathService::Get(base::FILE_EXE, &exe_path)) {
    std::wstring exe_subject = GetCertificateSubjectName(exe_path);
    if (!exe_subject.empty()) {
      expected_publisher = exe_subject;
    }
  }

  if (subject_name != expected_publisher) {
    LOG(ERROR) << "Subject mismatch. Expected: " << expected_publisher
               << ", got: " << subject_name;
    return false;
  }
  return true;
}

bool IsBinaryTrusted(const FilePath& binary_path,
                     bool verify_publisher,
                     bool force_verify_in_dev_builds) {
  if (!PathExists(binary_path)) {
    return false;
  }

  // Enforce signature verification by default only in official release branded
  // builds. In debug branded builds (such as Chrome Remote Desktop dev MSI
  // builds where is_chrome_branded = true) and non-branded builds, bypass
  // signature verification by default unless explicitly forced via
  // `force_verify_in_dev_builds`.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(NDEBUG)
  const bool should_verify = true;
#else
  const bool should_verify = force_verify_in_dev_builds;
#endif

  if (!should_verify) {
    return true;
  }

  ScopedWintrustData wintrust_data(binary_path);
  bool is_trusted = IsWintrustDataTrusted(wintrust_data, verify_publisher);
  if (is_trusted) {
    // Indicates that the binary is trusted:
    //   - The hash that represents the subject is trusted
    //   - The publisher is Google
    //   - No verification or time stamp chain errors
    VLOG(1) << "Verified signature and publisher for " << binary_path.value();
    return true;
  }

  if (wintrust_data.is_valid()) {
    LOG(ERROR)
        << "Binary signature valid but publisher verification failed for "
        << binary_path.value();
    return false;
  }

  LONG trust_status = wintrust_data.status();
  DWORD last_error = ::GetLastError();
  switch (trust_status) {
    case TRUST_E_NOSIGNATURE:
      // The file was not signed or had a signature that was not valid.
      // The reason for this status is retrieved via GetLastError(). Note that
      // the last error is a DWORD but the expected values set by this function
      // are HRESULTS so we need to cast.
      switch (static_cast<HRESULT>(last_error)) {
        case TRUST_E_NOSIGNATURE:
          PLOG(ERROR) << "No signature found for " << binary_path.value();
          break;
        case TRUST_E_SUBJECT_FORM_UNKNOWN:
          PLOG(ERROR) << "The trust provider does not support the form "
                      << "specified for the subject for "
                      << binary_path.value();
          break;
        case TRUST_E_PROVIDER_UNKNOWN:
          PLOG(ERROR) << "The trust provider is not recognized on this system "
                      << "for " << binary_path.value();
          break;
        default:
          // The signature was not valid or there was an error opening the file.
          PLOG(ERROR) << "Could not verify signature for "
                      << binary_path.value();
          break;
      }
      break;

    case TRUST_E_EXPLICIT_DISTRUST:
      // The hash that represents the subject or the publisher is not allowed by
      // the admin or user.
      LOG(ERROR) << "Signature for " << binary_path.value() << " is present, "
                 << "but is explicitly distrusted.";
      break;

    case TRUST_E_SUBJECT_NOT_TRUSTED:
      LOG(ERROR) << "Signature for " << binary_path.value() << " is present, "
                 << "but not trusted.";
      break;

    case CRYPT_E_SECURITY_SETTINGS:
      LOG(ERROR) << "Verification failed for " << binary_path.value() << ". "
                 << "The hash representing the subject or the publisher wasn't "
                 << "explicitly trusted by the admin and admin policy has "
                 << "disabled user trust. No signature, publisher or timestamp "
                 << "errors.";
      break;

    default:
      LOG(ERROR) << "Signature verification error for " << binary_path.value()
                 << ": status 0x" << std::hex << trust_status;
      break;
  }
  return false;
}

}  // namespace base::win
