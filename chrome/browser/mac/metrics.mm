// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/metrics.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>
#include <sys/attr.h>
#include <sys/vnode.h>
#include <unistd.h>

#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"

namespace {

mac_metrics::FileSystemType VolumeTagToFileSystemType(enum vtagtype tag) {
  static constexpr auto map =
      base::MakeFixedFlatMap<enum vtagtype, mac_metrics::FileSystemType>({
          {VT_HFS, mac_metrics::FileSystemType::kHFS},
          {VT_APFS, mac_metrics::FileSystemType::kAPFS},
      });
  const auto it = map.find(tag);
  return it != map.end() ? it->second : mac_metrics::FileSystemType::kUnknown;
}

void RecordAppFileSystemTypeUsingVolumeTag(enum vtagtype tag) {
  base::UmaHistogramEnumeration("Mac.AppFileSystemType",
                                VolumeTagToFileSystemType(tag));
}

void RecordAppUpgradeCodeSignatureValidationStatus(OSStatus status) {
  // There are currently 83 possible code signing errSec values.
  // https://github.com/apple-oss-distributions/Security/blob/Security-61040.80.10.0.1/OSX/libsecurity_codesigning/lib/CSCommon.h#L64
  base::UmaHistogramSparse("Mac.AppUpgradeCodeSignatureValidationStatus",
                           status);
}

void RecordAppUpgradeCodeSignatureValidationImpl(base::OnceClosure closure) {
  // SecCodeCheckValidity blocks on I/O, do the validation and metric recording
  // on from background thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceClosure closure) {
            SecCodeRef self_code = nullptr;
            if (SecCodeCopySelf(kSecCSDefaultFlags, &self_code) !=
                errSecSuccess) {
              std::move(closure).Run();
              return;
            }
            // Ignoring revocation status with the kSecCSNoNetworkAccess.
            RecordAppUpgradeCodeSignatureValidationStatus(SecCodeCheckValidity(
                self_code, kSecCSNoNetworkAccess, nullptr));
            if (self_code) {
              CFRelease(self_code);
            }

            std::move(closure).Run();
          },
          std::move(closure)));
}

struct alignas(4) AttributeBuffer {
  uint32_t length;
  enum vtagtype tag;
} __attribute__((packed));

}  // namespace

namespace mac_metrics {

Metrics::Metrics() {
  UpgradeDetector::GetInstance()->AddObserver(this);
}

Metrics::~Metrics() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);
}

void Metrics::RecordAppFileSystemType() {
  const char* path =
      NSProcessInfo.processInfo.arguments.firstObject.fileSystemRepresentation;

  struct attrlist attr_list = {
      .bitmapcount = ATTR_BIT_MAP_COUNT,  // default
      .commonattr = ATTR_CMN_OBJTAG       // get the file system type
  };
  struct AttributeBuffer buff;

  // Using getattrlist instead of statfs. ATTR_CMN_OBJTAG from getattrlist is
  // the only value needed, which should be faster to get than the whole statfs
  // struct. Additionally the statfs field f_type does not seem to map to any
  // public enum of file system types. According to man 2 getattrlist it is not
  // a useful value. The f_fstypename field could be used but adds the
  // additional burden of handling strings.
  if (getattrlist(path, &attr_list, &buff, sizeof(buff), 0) != 0) {
    // Record FileSystemType::kUnknown if there is a failure with getattrlist.
    RecordAppFileSystemTypeUsingVolumeTag(VT_NON);
    return;
  }
  DCHECK_GE(sizeof(buff), buff.length);
  RecordAppFileSystemTypeUsingVolumeTag(buff.tag);
}

void Metrics::OnUpgradeRecommended() {
  // By default OnUpgradeRecommended is called multiple times over the course
  // of 7 days, the first being 1 hour after the update has been staged. Record
  // the metric once and ignore all other calls.
  static bool once = []() {
    RecordAppUpgradeCodeSignatureValidationImpl(base::DoNothing());
    return true;
  }();
  DCHECK(once);
}

void Metrics::RecordAppUpgradeCodeSignatureValidation(
    base::OnceClosure closure) {
  RecordAppUpgradeCodeSignatureValidationImpl(std::move(closure));
}

}  // namespace mac_metrics
