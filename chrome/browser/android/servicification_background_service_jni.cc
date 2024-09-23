// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/system/sys_info.h"
#include "chrome/android/test_support_jni_headers/ServicificationBackgroundService_jni.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "components/metrics/persistent_system_profile.h"
#include "components/variations/active_field_trials.h"
#include "third_party/metrics_proto/system_profile.pb.h"

// Verifies that the memory-mapped file for persistent histograms data exists
// and contains a valid SystemProfile.
jboolean
JNI_ServicificationBackgroundService_TestPersistentHistogramsOnDiskSystemProfile(
    JNIEnv* env) {
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  if (!allocator) {
    LOG(ERROR) << "The GlobalHistogramAllocator is null!";
    return false;
  }

  const base::FilePath& persistent_file_path =
      allocator->GetPersistentLocation();
  if (persistent_file_path.empty()) {
    LOG(ERROR) << "The path of the persistent file is empty!";
    return false;
  }

  base::File file(persistent_file_path,
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    LOG(ERROR) << "The persistent file isn't valid!";
    return false;
  }

  std::unique_ptr<base::MemoryMappedFile> mapped(new base::MemoryMappedFile());
  if (!mapped->Initialize(std::move(file), base::MemoryMappedFile::READ_ONLY)) {
    LOG(ERROR) << "Fails to Initialize the memory mapped file!";
    return false;
  }

  if (!base::FilePersistentMemoryAllocator::IsFileAcceptable(
          *mapped, true /* read_only */)) {
    LOG(ERROR) << "The memory mapped file isn't acceptable!";
    return false;
  }

  // Map the file and validate it.
  std::unique_ptr<base::FilePersistentMemoryAllocator> memory_allocator =
      std::make_unique<base::FilePersistentMemoryAllocator>(
          std::move(mapped), 0, 0, std::string_view(),
          base::FilePersistentMemoryAllocator::kReadOnly);
  if (memory_allocator->GetMemoryState() ==
      base::PersistentMemoryAllocator::MEMORY_DELETED) {
    LOG(ERROR) << "The memory allocator state shouldn't be MEMORY_DELETED!";
    return false;
  }

  if (memory_allocator->IsCorrupt()) {
    LOG(ERROR) << "The memory allocator is corrupt!";
    return false;
  }

  if (!metrics::PersistentSystemProfile::HasSystemProfile(*memory_allocator)) {
    LOG(ERROR) << "There isn't a System Profile!";
    return false;
  }

  metrics::SystemProfileProto system_profile_proto;
  if (!metrics::PersistentSystemProfile::GetSystemProfile(
          *memory_allocator, &system_profile_proto)) {
    LOG(ERROR) << "Failed to get the System Profile!";
    return false;
  }

  if (!system_profile_proto.has_os()) {
    LOG(ERROR) << "The os info isn't set!";
    return false;
  }

  const metrics::SystemProfileProto_OS& os = system_profile_proto.os();
  if (!os.has_version()) {
    LOG(ERROR) << "The os version isn't set!";
    return false;
  }

  if (base::SysInfo::OperatingSystemVersion().compare(os.version()) != 0) {
    LOG(ERROR) << "The os version doesn't match!";
    return false;
  }

  std::vector<variations::ActiveGroupId> field_trial_ids;
  // The persistent histograms on disk do not include low anonymity trials (as
  // we do not include those in metrics reports).
  variations::GetFieldTrialActiveGroupIds("", &field_trial_ids);
  int expected_size = static_cast<int>(field_trial_ids.size());

  // The active field trial "Foo" is guaranteed in the list.
  if (expected_size <= 0) {
    LOG(ERROR) << "Expect at least one active field trial!";
    return false;
  }

  if (system_profile_proto.field_trial_size() != expected_size) {
    LOG(ERROR) << "The size of field trials recorded in the System Profile"
               << " doesn't match the size of active field trials!";
    return false;
  }

  return true;
}

// Returns whether a background session has started.
jboolean JNI_ServicificationBackgroundService_IsBackgroundSessionStart(
    JNIEnv* env) {
  return UmaSessionStats::IsBackgroundSessionStartForTesting();
}
