// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_KCER_HISTOGRAMS_H_
#define ASH_COMPONENTS_KCER_KCER_HISTOGRAMS_H_

#include "ash/components/kcer/kcer.h"
#include "base/component_export.h"

namespace kcer {

namespace internal {
const char KcerPkcs12ImportMetrics[] = "ChromeOS.Kcer.KcerPkcs12ImportEvent";

// Events related to import of PKCS#12 files using
// "kcer_token_impl_nss.h". These values are persisted to
// histograms. Entries should not be renumbered and numeric values should never
// be reused.
enum class KcerPkcs12ImportEvent {
  AttemptedPkcs12ChapsImport = 0,
  AttemptedPkcs12ChapsImportTask = 1,
  SuccessPkcs12ChapsImport = 2,
  AttemptedRsaKeyImportTask = 3,
  SuccessRsaKeyImportTask = 4,
  SuccessRsaCertImportTask = 5,
  AttemptedEcKeyImportTask = 6,
  SuccessEcKeyImportTask = 7,
  SuccessEcCertImportTask = 8,
  AttemptedMultipleCertImport = 9,
  SuccessMultipleCertImport = 10,
  kMaxValue = SuccessMultipleCertImport,
};

void RecordKcerPkcs12ImportUmaEvent(KcerPkcs12ImportEvent event);

}  // namespace internal

// Events related to import of PKCS#12 files. These values are persisted to
// histograms. Entries should not be renumbered and numeric values should never
// be reused.
enum class Pkcs12MigrationUmaEvent {
  kPkcs12ImportNssSuccess = 0,
  kPkcs12ImportNssFailed = 1,
  kPkcs12ImportKcerSuccess = 2,
  kPkcs12ImportKcerFailed = 3,
  kMaxValue = kPkcs12ImportKcerFailed,
};

COMPONENT_EXPORT(KCER)
void RecordPkcs12MigrationUmaEvent(Pkcs12MigrationUmaEvent event);

COMPONENT_EXPORT(KCER)
void RecordKcerError(Error error);

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_KCER_HISTOGRAMS_H_
