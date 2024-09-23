// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/binder_box.h"

#include <android/binder_ibinder.h>
#include <jni.h>

#include <cstddef>
#include <utility>
#include <vector>

#include "base/android/binder.h"
#include "base/android/scoped_java_ref.h"
#include "base/types/expected_macros.h"

namespace base::android {

namespace {

DEFINE_BINDER_CLASS(BinderBoxInterface);

// Binder transaction support for PackBinderBox() and UnpackBinderBox().
class BinderBox : public SupportsBinder<BinderBoxInterface> {
  static constexpr transaction_code_t kUnpack = 1;

 public:
  explicit BinderBox(std::vector<BinderRef> binders)
      : binders_(std::move(binders)) {}

  ScopedJavaLocalRef<jobject> GetJavaBinder(JNIEnv* env) {
    return GetBinder().ToJavaBinder(env);
  }

  static BinderStatusOr<std::vector<BinderRef>> Unpack(
      JNIEnv* env,
      const JavaRef<jobject>& box) {
    auto proxy = TypedBinderRef<BinderBoxInterface>::Adopt(
        BinderRef::FromJavaBinder(env, box.obj()));
    if (!proxy) {
      return unexpected(STATUS_BAD_TYPE);
    }
    ASSIGN_OR_RETURN(auto parcel, proxy.PrepareTransaction());
    ASSIGN_OR_RETURN(const auto reply,
                     proxy.Transact(kUnpack, std::move(parcel)));
    ASSIGN_OR_RETURN(const size_t num_binders, reply.reader().ReadUint32());
    std::vector<BinderRef> binders(num_binders);
    for (size_t i = 0; i < num_binders; ++i) {
      ASSIGN_OR_RETURN(binders[i], reply.reader().ReadBinder());
    }
    return binders;
  }

 private:
  ~BinderBox() override = default;

  // SupportsBinder<BinderBoxInterface>:
  BinderStatusOr<void> OnBinderTransaction(transaction_code_t code,
                                           const ParcelReader& in,
                                           const ParcelWriter& out) override {
    if (code != kUnpack) {
      return unexpected(STATUS_UNKNOWN_TRANSACTION);
    }
    const uint32_t num_binders = checked_cast<uint32_t>(binders_.size());
    RETURN_IF_ERROR(out.WriteUint32(num_binders));
    for (uint32_t i = 0; i < num_binders; ++i) {
      RETURN_IF_ERROR(out.WriteBinder(binders_[i]));
    }
    binders_.clear();
    return ok();
  }

  std::vector<BinderRef> binders_;
};

}  // namespace

ScopedJavaLocalRef<jobject> PackBinderBox(JNIEnv* env,
                                          std::vector<BinderRef> binders) {
  if (binders.empty()) {
    return nullptr;
  }
  return MakeRefCounted<BinderBox>(std::move(binders))->GetJavaBinder(env);
}

BinderStatusOr<std::vector<BinderRef>> UnpackBinderBox(
    JNIEnv* env,
    const JavaRef<jobject>& box) {
  return BinderBox::Unpack(env, box);
}

}  // namespace base::android
