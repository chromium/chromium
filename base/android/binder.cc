// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/binder.h"

#include <android/binder_ibinder.h>
#include <android/binder_ibinder_jni.h>
#include <android/binder_parcel.h>
#include <android/binder_status.h>
#include <dlfcn.h>

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/android/requires_api.h"
#include "base/check.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"

// The Binder NDK library was introduced in Q.
#define BINDER_MIN_API 29

// Helper used extensively herein to guard blocks of code on the availability of
// Binder NDK functions.
#define WITH_BINDER_API(name)                         \
  if (__builtin_available(android BINDER_MIN_API, *)) \
    if (GetBinderApi())                               \
      if (const BinderApi& name = *GetBinderApi(); true)

namespace base::android {

namespace {

// Helper to expose useful functions from libbinder_ndk.so at runtime. Currently
// limited to functions supported in Q.
struct BASE_EXPORT BinderApi {
  // Excluded from raw_ptr because this is trivially safe and it keeps BinderApi
  // from having a destructor in any build configuration.
  RAW_PTR_EXCLUSION void* const library = dlopen("libbinder_ndk.so", RTLD_LAZY);

#define DEFINE_BINDER_API_ENTRY(name)  \
  REQUIRES_ANDROID_API(BINDER_MIN_API) \
  decltype(::name)* const name =       \
      library ? (decltype(::name)*)dlsym(library, "" #name) : nullptr

  DEFINE_BINDER_API_ENTRY(AIBinder_Class_define);
  DEFINE_BINDER_API_ENTRY(AIBinder_Class_setOnDump);
  DEFINE_BINDER_API_ENTRY(AIBinder_new);
  DEFINE_BINDER_API_ENTRY(AIBinder_isRemote);
  DEFINE_BINDER_API_ENTRY(AIBinder_isAlive);
  DEFINE_BINDER_API_ENTRY(AIBinder_ping);
  DEFINE_BINDER_API_ENTRY(AIBinder_dump);
  DEFINE_BINDER_API_ENTRY(AIBinder_linkToDeath);
  DEFINE_BINDER_API_ENTRY(AIBinder_unlinkToDeath);
  DEFINE_BINDER_API_ENTRY(AIBinder_getCallingUid);
  DEFINE_BINDER_API_ENTRY(AIBinder_getCallingPid);
  DEFINE_BINDER_API_ENTRY(AIBinder_incStrong);
  DEFINE_BINDER_API_ENTRY(AIBinder_decStrong);
  DEFINE_BINDER_API_ENTRY(AIBinder_debugGetRefCount);
  DEFINE_BINDER_API_ENTRY(AIBinder_associateClass);
  DEFINE_BINDER_API_ENTRY(AIBinder_getClass);
  DEFINE_BINDER_API_ENTRY(AIBinder_getUserData);
  DEFINE_BINDER_API_ENTRY(AIBinder_prepareTransaction);
  DEFINE_BINDER_API_ENTRY(AIBinder_transact);
  DEFINE_BINDER_API_ENTRY(AIBinder_Weak_new);
  DEFINE_BINDER_API_ENTRY(AIBinder_Weak_delete);
  DEFINE_BINDER_API_ENTRY(AIBinder_Weak_promote);
  DEFINE_BINDER_API_ENTRY(AIBinder_DeathRecipient_new);
  DEFINE_BINDER_API_ENTRY(AIBinder_DeathRecipient_delete);
  DEFINE_BINDER_API_ENTRY(AIBinder_fromJavaBinder);
  DEFINE_BINDER_API_ENTRY(AIBinder_toJavaBinder);
  DEFINE_BINDER_API_ENTRY(AParcel_delete);
  DEFINE_BINDER_API_ENTRY(AParcel_setDataPosition);
  DEFINE_BINDER_API_ENTRY(AParcel_getDataPosition);
  DEFINE_BINDER_API_ENTRY(AParcel_writeStrongBinder);
  DEFINE_BINDER_API_ENTRY(AParcel_readStrongBinder);
  DEFINE_BINDER_API_ENTRY(AParcel_writeParcelFileDescriptor);
  DEFINE_BINDER_API_ENTRY(AParcel_readParcelFileDescriptor);
  DEFINE_BINDER_API_ENTRY(AParcel_writeStatusHeader);
  DEFINE_BINDER_API_ENTRY(AParcel_readStatusHeader);
  DEFINE_BINDER_API_ENTRY(AParcel_writeString);
  DEFINE_BINDER_API_ENTRY(AParcel_readString);
  DEFINE_BINDER_API_ENTRY(AParcel_writeStringArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readStringArray);
  DEFINE_BINDER_API_ENTRY(AParcel_writeParcelableArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readParcelableArray);
  DEFINE_BINDER_API_ENTRY(AParcel_writeInt32);
  DEFINE_BINDER_API_ENTRY(AParcel_writeUint32);
  DEFINE_BINDER_API_ENTRY(AParcel_writeInt64);
  DEFINE_BINDER_API_ENTRY(AParcel_writeUint64);
  DEFINE_BINDER_API_ENTRY(AParcel_writeFloat);
  DEFINE_BINDER_API_ENTRY(AParcel_writeDouble);
  DEFINE_BINDER_API_ENTRY(AParcel_writeBool);
  DEFINE_BINDER_API_ENTRY(AParcel_writeChar);
  DEFINE_BINDER_API_ENTRY(AParcel_writeByte);
  DEFINE_BINDER_API_ENTRY(AParcel_readInt32);
  DEFINE_BINDER_API_ENTRY(AParcel_readUint32);
  DEFINE_BINDER_API_ENTRY(AParcel_readInt64);
  DEFINE_BINDER_API_ENTRY(AParcel_readUint64);
  DEFINE_BINDER_API_ENTRY(AParcel_readFloat);
  DEFINE_BINDER_API_ENTRY(AParcel_readDouble);
  DEFINE_BINDER_API_ENTRY(AParcel_readBool);
  DEFINE_BINDER_API_ENTRY(AParcel_readChar);
  DEFINE_BINDER_API_ENTRY(AParcel_readByte);
  DEFINE_BINDER_API_ENTRY(AParcel_writeInt32Array);
  DEFINE_BINDER_API_ENTRY(AParcel_writeUint32Array);
  DEFINE_BINDER_API_ENTRY(AParcel_writeInt64Array);
  DEFINE_BINDER_API_ENTRY(AParcel_writeUint64Array);
  DEFINE_BINDER_API_ENTRY(AParcel_writeFloatArray);
  DEFINE_BINDER_API_ENTRY(AParcel_writeDoubleArray);
  DEFINE_BINDER_API_ENTRY(AParcel_writeBoolArray);
  DEFINE_BINDER_API_ENTRY(AParcel_writeCharArray);
  DEFINE_BINDER_API_ENTRY(AParcel_writeByteArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readInt32Array);
  DEFINE_BINDER_API_ENTRY(AParcel_readUint32Array);
  DEFINE_BINDER_API_ENTRY(AParcel_readInt64Array);
  DEFINE_BINDER_API_ENTRY(AParcel_readUint64Array);
  DEFINE_BINDER_API_ENTRY(AParcel_readFloatArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readDoubleArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readBoolArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readCharArray);
  DEFINE_BINDER_API_ENTRY(AParcel_readByteArray);
#undef DEFINE_BINDER_API_ENTRY
};

static BinderApi* GetBinderApi() {
  static BinderApi api;
  if (!api.library) {
    return nullptr;
  }
  return &api;
}

std::unique_ptr<std::vector<BinderRef>>& BindersFromParent() {
  static NoDestructor<std::unique_ptr<std::vector<BinderRef>>> ptr;
  return *ptr;
}

}  // namespace

ParcelReader::ParcelReader(const AParcel* parcel) : parcel_(parcel) {}

ParcelReader::ParcelReader(const Parcel& parcel) : parcel_(parcel.get()) {}

ParcelReader::ParcelReader(const ParcelReader&) = default;

ParcelReader& ParcelReader::operator=(const ParcelReader&) = default;

ParcelReader::~ParcelReader() = default;

BinderStatusOr<BinderRef> ParcelReader::ReadBinder() const {
  WITH_BINDER_API(api) {
    AIBinder* binder;
    const auto status = api.AParcel_readStrongBinder(parcel_.get(), &binder);
    if (status != STATUS_OK) {
      return unexpected(status);
    }
    return BinderRef(binder);
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

BinderStatusOr<int32_t> ParcelReader::ReadInt32() const {
  WITH_BINDER_API(api) {
    int32_t value;
    const auto status = api.AParcel_readInt32(parcel_.get(), &value);
    if (status != STATUS_OK) {
      return unexpected(status);
    }
    return ok(value);
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

BinderStatusOr<uint32_t> ParcelReader::ReadUint32() const {
  WITH_BINDER_API(api) {
    uint32_t value;
    const auto status = api.AParcel_readUint32(parcel_.get(), &value);
    if (status != STATUS_OK) {
      return unexpected(status);
    }
    return ok(value);
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

BinderStatusOr<uint64_t> ParcelReader::ReadUint64() const {
  WITH_BINDER_API(api) {
    uint64_t value;
    const auto status = api.AParcel_readUint64(parcel_.get(), &value);
    if (status != STATUS_OK) {
      return unexpected(status);
    }
    return ok(value);
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

BinderStatusOr<void> ParcelReader::ReadByteArrayImpl(
    AParcel_byteArrayAllocator allocator,
    void* context) const {
  WITH_BINDER_API(api) {
    const auto status =
        api.AParcel_readByteArray(parcel_.get(), context, allocator);
    if (status != STATUS_OK) {
      return unexpected(status);
    }
    return ok();
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

BinderStatusOr<ScopedFD> ParcelReader::ReadFileDescriptor() const {
  WITH_BINDER_API(api) {
    int fd;
    const auto status =
        api.AParcel_readParcelFileDescriptor(parcel_.get(), &fd);
    if (status != STATUS_OK) {
      return unexpected(status);
    }
    return ScopedFD(fd);
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

ParcelWriter::ParcelWriter(AParcel* parcel) : parcel_(parcel) {}

ParcelWriter::ParcelWriter(Parcel& parcel) : parcel_(parcel.get()) {}

ParcelWriter::ParcelWriter(const ParcelWriter&) = default;

ParcelWriter& ParcelWriter::operator=(const ParcelWriter&) = default;

ParcelWriter::~ParcelWriter() = default;

BinderStatusOr<void> ParcelWriter::WriteBinder(BinderRef binder) const {
  binder_status_t status = STATUS_UNEXPECTED_NULL;
  WITH_BINDER_API(api) {
    status = api.AParcel_writeStrongBinder(parcel_.get(), binder.get());
    if (status == STATUS_OK) {
      return ok();
    }
  }
  return unexpected(status);
}

BinderStatusOr<void> ParcelWriter::WriteInt32(int32_t value) const {
  binder_status_t status = STATUS_UNEXPECTED_NULL;
  WITH_BINDER_API(api) {
    status = api.AParcel_writeInt32(parcel_.get(), value);
    if (status == STATUS_OK) {
      return ok();
    }
  }
  return unexpected(status);
}

BinderStatusOr<void> ParcelWriter::WriteUint32(uint32_t value) const {
  binder_status_t status = STATUS_UNEXPECTED_NULL;
  WITH_BINDER_API(api) {
    status = api.AParcel_writeUint32(parcel_.get(), value);
    if (status == STATUS_OK) {
      return ok();
    }
  }
  return unexpected(status);
}

BinderStatusOr<void> ParcelWriter::WriteUint64(uint64_t value) const {
  binder_status_t status = STATUS_UNEXPECTED_NULL;
  WITH_BINDER_API(api) {
    status = api.AParcel_writeUint64(parcel_.get(), value);
    if (status == STATUS_OK) {
      return ok();
    }
  }
  return unexpected(status);
}

BinderStatusOr<void> ParcelWriter::WriteByteArray(
    span<const uint8_t> bytes) const {
  binder_status_t status = STATUS_UNEXPECTED_NULL;
  WITH_BINDER_API(api) {
    status = api.AParcel_writeByteArray(
        parcel_.get(), reinterpret_cast<const int8_t*>(bytes.data()),
        checked_cast<int32_t>(bytes.size()));
    if (status == STATUS_OK) {
      return ok();
    }
  }
  return unexpected(status);
}

BinderStatusOr<void> ParcelWriter::WriteFileDescriptor(ScopedFD file) const {
  binder_status_t status = STATUS_UNEXPECTED_NULL;
  WITH_BINDER_API(api) {
    status = api.AParcel_writeParcelFileDescriptor(parcel_.get(), file.get());
    if (status == STATUS_OK) {
      return ok();
    }
  }
  return unexpected(status);
}

Parcel::Parcel() = default;

Parcel::Parcel(AParcel* parcel) : parcel_(parcel) {}

Parcel::Parcel(Parcel&& other) : parcel_(other.release()) {}

Parcel& Parcel::operator=(Parcel&& other) {
  reset();
  parcel_ = other.release();
  return *this;
}

Parcel::~Parcel() {
  reset();
}

void Parcel::reset() {
  WITH_BINDER_API(api) {
    if (AParcel* parcel = release()) {
      api.AParcel_delete(parcel);
    }
  }
}

BinderRef::BinderRef() = default;

BinderRef::BinderRef(AIBinder* binder) : binder_(binder) {}

BinderRef::BinderRef(const BinderRef& other) : binder_(other.binder_) {
  if (binder_) {
    WITH_BINDER_API(api) {
      api.AIBinder_incStrong(binder_);
    }
  }
}

BinderRef& BinderRef::operator=(const BinderRef& other) {
  reset();
  binder_ = other.binder_;
  if (binder_) {
    WITH_BINDER_API(api) {
      api.AIBinder_incStrong(binder_);
    }
  }
  return *this;
}

BinderRef::BinderRef(BinderRef&& other) : binder_(other.release()) {}

BinderRef& BinderRef::operator=(BinderRef&& other) {
  reset();
  binder_ = other.release();
  return *this;
}

BinderRef::~BinderRef() {
  reset();
}

void BinderRef::reset() {
  if (AIBinder* binder = release()) {
    WITH_BINDER_API(api) {
      api.AIBinder_decStrong(binder);
    }
  }
}

ScopedJavaLocalRef<jobject> BinderRef::ToJavaBinder(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> object;
  if (binder_) {
    WITH_BINDER_API(api) {
      object = ScopedJavaLocalRef<jobject>::Adopt(
          env, api.AIBinder_toJavaBinder(env, binder_.get()));
    }
  }
  return object;
}

BinderRef BinderRef::FromJavaBinder(JNIEnv* env, jobject java_binder) {
  WITH_BINDER_API(api) {
    if (AIBinder* binder = api.AIBinder_fromJavaBinder(env, java_binder)) {
      return BinderRef(binder);
    }
  }
  return BinderRef();
}

bool BinderRef::AssociateWithClass(AIBinder_Class* binder_class) {
  if (binder_) {
    WITH_BINDER_API(api) {
      return api.AIBinder_associateClass(binder_.get(), binder_class);
    }
  }
  return false;
}

BinderStatusOr<Parcel> BinderRef::PrepareTransaction() {
  if (binder_) {
    WITH_BINDER_API(api) {
      AParcel* parcel;
      const auto status =
          api.AIBinder_prepareTransaction(binder_.get(), &parcel);
      if (status != STATUS_OK) {
        return unexpected(status);
      }
      return Parcel(parcel);
    }
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

BinderStatusOr<Parcel> BinderRef::TransactImpl(transaction_code_t code,
                                               Parcel parcel,
                                               binder_flags_t flags) {
  if (binder_) {
    WITH_BINDER_API(api) {
      // NOTE: AIBinder_transact always takes ownership of the input parcel even
      // in failure modes. Hence it's safe to release here unconditionally.
      AParcel* in = parcel.release();
      AParcel* out;
      const auto status =
          api.AIBinder_transact(binder_.get(), code, &in, &out, flags);
      if (status != STATUS_OK) {
        return unexpected(status);
      }
      return Parcel(out);
    }
  }
  return unexpected(STATUS_UNEXPECTED_NULL);
}

namespace internal {

AIBinder_Class* BinderClassBase::RegisterBinderClass(const char* name) {
  WITH_BINDER_API(api) {
    return api.AIBinder_Class_define(name, &SupportsBinderBase::OnIBinderCreate,
                                     &SupportsBinderBase::OnIBinderDestroy,
                                     &SupportsBinderBase::OnIBinderTransact);
  }
  return nullptr;
}

SupportsBinderBase::SupportsBinderBase(AIBinder_Class* binder_class)
    : binder_class_(binder_class) {}

SupportsBinderBase::~SupportsBinderBase() {
#if DCHECK_IS_ON()
  // If we're being destroyed there must no longer be an IBinder for this
  // object. And in that case, `weak_binder_` should have already been cleared
  // by OnIBinderDestroy().
  AutoLock lock(lock_);
  DCHECK(!weak_binder_);
#endif
}

BinderRef SupportsBinderBase::GetBinder() {
  WITH_BINDER_API(api) {
    AutoLock lock(lock_);
    if (weak_binder_) {
      AIBinder* strong = api.AIBinder_Weak_promote(weak_binder_.get());
      if (strong) {
        return BinderRef(strong);
      }

      // Our weak IBinder is no longer valid.
      api.AIBinder_Weak_delete(weak_binder_.get());
      weak_binder_ = nullptr;
    }

    // We have no IBinder, so create a new one.
    AIBinder* binder = api.AIBinder_new(binder_class_.get(), this);
    CHECK(binder);
    weak_binder_ = api.AIBinder_Weak_new(binder);
    self_for_binder_ = this;
    return BinderRef(binder);
  }

  return BinderRef();
}

void SupportsBinderBase::OnBinderDestroyed() {}

void SupportsBinderBase::OnBinderDestroyedBase() {
  scoped_refptr<SupportsBinderBase> self_ref;
  WITH_BINDER_API(api) {
    AutoLock lock(lock_);
    if (weak_binder_) {
      api.AIBinder_Weak_delete(weak_binder_.get());
      weak_binder_ = nullptr;
    }
    self_ref.swap(self_for_binder_);
  }
  OnBinderDestroyed();

  // May delete `this`.
  self_ref.reset();
}

void* SupportsBinderBase::OnIBinderCreate(void* self) {
  return self;
}

void SupportsBinderBase::OnIBinderDestroy(void* self) {
  reinterpret_cast<SupportsBinderBase*>(self)->OnBinderDestroyedBase();
}

binder_status_t SupportsBinderBase::OnIBinderTransact(AIBinder* binder,
                                                      transaction_code_t code,
                                                      const AParcel* in,
                                                      AParcel* out) {
  WITH_BINDER_API(api) {
    void* const user_data = api.AIBinder_getUserData(binder);
    auto* const target = reinterpret_cast<SupportsBinderBase*>(user_data);

    const auto result =
        target->OnBinderTransaction(code, ParcelReader(in), ParcelWriter(out));
    return result.has_value() ? STATUS_OK : result.error();
  }

  // If binder NDK is unsupported, nobody will be calling this method.
  NOTREACHED();
}

}  // namespace internal

bool IsNativeBinderAvailable() {
  return GetBinderApi();
}

void SetBindersFromParent(std::vector<BinderRef> binders) {
  CHECK(!BindersFromParent());
  BindersFromParent() =
      std::make_unique<std::vector<BinderRef>>(std::move(binders));
}

BinderRef TakeBinderFromParent(size_t index) {
  auto& binders = BindersFromParent();
  CHECK(binders);
  if (index >= binders->size()) {
    return BinderRef();
  }
  return std::move(binders->at(index));
}

}  // namespace base::android
