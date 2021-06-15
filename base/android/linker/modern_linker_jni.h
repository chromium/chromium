// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_LINKER_MODERN_LINKER_JNI_H_
#define BASE_ANDROID_LINKER_MODERN_LINKER_JNI_H_

#include <jni.h>
#include <link.h>

namespace chromium_android_linker {

class String;

// Used to find out whether RELRO sharing is often rejected due to mismatch of
// the contents.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Must be kept in sync with the enum
// in enums.xml. A java @IntDef is generated from this.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base.library_loader
enum class RelroSharingStatus {
  NOT_ATTEMPTED = 0,
  SHARED = 1,
  NOT_IDENTICAL = 2,
  COUNT = 3,
};

struct SharedMemoryFunctions;

// Holds address ranges of the loaded native library, its RELRO region, along
// with the RELRO FD identifying the shared memory region. Carries the same
// members as the Java-side LibInfo (without mLibFilePath), allowing to
// internally import/export the member values from/to the Java-side counterpart.
//
// Does *not* own the RELRO FD as soon as the latter gets exported to Java
// (as a result of 'spawning' the RELRO region as shared memory.
//
// *Not* threadsafe.
class NativeLibInfo {
 public:
  // Constructs an empty instance. The |java_object| indicates the handle to
  // import and export member fields.
  //
  // Having |env| as |nullptr| disables export to java for the lifetime of the
  // instance. This is useful as a scratch info that is gradually populated for
  // comparison with another NativeLibInfo, and then discarded.
  NativeLibInfo(JNIEnv* env, jobject java_object);

  // Copies the java-side object state to this native instance. Returns false
  // iff an imported value is invalid.
  bool CopyFromJavaObject();

  void set_load_address(uintptr_t a) { load_address_ = a; }

  uintptr_t load_address() const { return load_address_; }

  // Loads the native library using android_dlopen_ext and invokes JNI_OnLoad().
  //
  // On a successful load exports the address range of the library to the
  // Java-side LibInfo.
  //
  // Iff |spawn_relro_region| is true, also finds the RELRO region in the
  // library (PT_GNU_RELRO), converts it to be backed by a shared memory region
  // (here referred as "RELRO FD") and exports the RELRO information to Java
  // (the address range and the RELRO FD).
  //
  // When spawned, the shared memory region is exported only after sealing as
  // read-only and without writable memory mappings. This allows any process to
  // provide RELRO FD before it starts processing arbitrary input. For example,
  // an App Zygote can create a RELRO FD in a sufficiently trustworthy way to
  // make the Browser/Privileged processes share the region with it.
  bool LoadLibrary(const String& library_path, bool spawn_relro_region);

  // Finds the RELRO region in the native library identified by
  // |this->load_address()| and replaces it with the shared memory region
  // identified by |other_lib_info|.
  //
  // The external NativeLibInfo can arrive from a different process.
  //
  // Note on security: The RELRO region is treated as *trusted*, no untrusted
  // user/website/network input can be processed in an isolated process before
  // it sends the RELRO FD. This is because there is no way to check whether the
  // process has a writable mapping of the region remaining.
  bool CompareRelroAndReplaceItBy(const NativeLibInfo& other_lib_info);

  void set_relro_info_for_testing(uintptr_t start, size_t size) {
    relro_start_ = start;
    relro_size_ = size;
  }

  // Creates a shared RELRO region as it normally would during LoadLibrary()
  // with |spawn_relro_region=true|. Exposed here because it is difficult to
  // unittest LoadLibrary() directly.
  bool CreateSharedRelroFdForTesting();

  int get_relro_fd_for_testing() { return relro_fd_; }

  static bool SharedMemoryFunctionsSupportedForTesting();

 private:
  NativeLibInfo() = delete;

  // Not copyable or movable.
  NativeLibInfo(const NativeLibInfo&) = delete;
  NativeLibInfo& operator=(const NativeLibInfo&) = delete;

  // Exports the address range of the library described by |this| to the
  // Java-side LibInfo.
  void ExportLoadInfoToJava() const;

  // Exports the address range of the RELRO region and RELRO FD described by
  // |this| to the Java-side LibInfo.
  void ExportRelroInfoToJava() const;

  void CloseRelroFd();

  // Callback for dl_iterate_phdr(). From program headers (phdr(s)) of a loaded
  // library determines its load address, and in case it is equal to
  // |lib_info.load_address()|, extracts the RELRO and size information from
  // corresponding phdr(s).
  static int VisitLibraryPhdrs(dl_phdr_info* info, size_t size, void* lib_info);

  // Invokes dl_iterate_phdr() for the current load address, with
  // VisitLibraryPhdrs() as a callback.
  bool FindRelroAndLibraryRangesInElf();

  // Loads and initializes the load address ranges: |load_address_|,
  // |load_size_|. Assumes that the memory range is reserved (in Linker.java).
  bool LoadWithDlopenExt(const String& path, void** handle);

  // Initializes |relro_fd_| with a newly created read-only shared memory region
  // sized as the library's RELRO and with identical data.
  bool CreateSharedRelroFd(const SharedMemoryFunctions& functions);

  // Assuming that RELRO-related information is populated, memory-maps the RELRO
  // FD on top of the library's RELRO.
  bool ReplaceRelroWithSharedOne(const SharedMemoryFunctions& functions) const;

  // Returns true iff the RELRO address and size, along with the contents are
  // equal among the two.
  bool RelroIsIdentical(const NativeLibInfo& external_lib_info,
                        const SharedMemoryFunctions& functions) const;

  static constexpr int kInvalidFd = -1;
  uintptr_t load_address_ = 0;
  size_t load_size_ = 0;
  uintptr_t relro_start_ = 0;
  size_t relro_size_ = 0;
  int relro_fd_ = kInvalidFd;
  JNIEnv* const env_;
  const jobject java_object_;
};

// JNI_OnLoad() initialization hook for the modern linker.
// Sets up JNI and other initializations for native linker code.
// |vm| is the Java VM handle passed to JNI_OnLoad().
// |env| is the current JNI environment handle.
// On success, returns true.
extern bool ModernLinkerJNIInit(JavaVM* vm, JNIEnv* env);

}  // namespace chromium_android_linker

#endif  // BASE_ANDROID_LINKER_MODERN_LINKER_JNI_H_
