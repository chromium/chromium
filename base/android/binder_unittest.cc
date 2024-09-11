// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/binder.h"

#include <android/binder_status.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/types/expected_macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace base::android {
namespace {

class BinderTest : public testing::Test {
 public:
  void SetUp() override {
    if (!IsNativeBinderAvailable()) {
      GTEST_SKIP() << "This test is only meaningful when run on Android Q+ "
                   << "with the binder NDK library available.";
    }
  }
};

DEFINE_BINDER_CLASS(ReflectorClass);

class Reflector : public SupportsBinder<ReflectorClass> {
 public:
  using ReflectCallback = OnceCallback<void(const ParcelReader&)>;

  template <typename WriteFn, typename ReadFn>
  void Reflect(WriteFn writer, ReadFn reader) {
    auto binder = GetBinder();
    auto parcel = *binder.PrepareTransaction();
    writer(parcel.writer());
    reflected_.Reset();
    reflect_ = BindLambdaForTesting(reader);
    EXPECT_TRUE(binder.TransactOneWay(1, std::move(parcel)).has_value());
    reflected_.Wait();
  }

 private:
  ~Reflector() override = default;

  // SupportsBinder<ReflectorClass>:
  BinderStatusOr<void> OnBinderTransaction(transaction_code_t code,
                                           const ParcelReader& reader,
                                           const ParcelWriter& out) override {
    std::move(reflect_).Run(reader);
    reflected_.Signal();
    return ok();
  }

  ReflectCallback reflect_;
  WaitableEvent reflected_;
};

TEST_F(BinderTest, ReadWriteInt32) {
  auto reflector = MakeRefCounted<Reflector>();
  reflector->Reflect(
      [](const ParcelWriter& writer) {
        EXPECT_TRUE(writer.WriteInt32(42).has_value());
        EXPECT_TRUE(writer.WriteInt32(-42).has_value());
        EXPECT_TRUE(
            writer.WriteInt32(std::numeric_limits<int32_t>::min()).has_value());
        EXPECT_TRUE(
            writer.WriteInt32(std::numeric_limits<int32_t>::max()).has_value());
      },
      [](const ParcelReader& reader) {
        EXPECT_EQ(42, *reader.ReadInt32());
        EXPECT_EQ(-42, *reader.ReadInt32());
        EXPECT_EQ(std::numeric_limits<int32_t>::min(), *reader.ReadInt32());
        EXPECT_EQ(std::numeric_limits<int32_t>::max(), *reader.ReadInt32());
      });
}

TEST_F(BinderTest, ReadWriteUint32) {
  auto reflector = MakeRefCounted<Reflector>();
  reflector->Reflect(
      [](const ParcelWriter& writer) {
        EXPECT_TRUE(writer.WriteUint32(42).has_value());
        EXPECT_TRUE(writer.WriteUint32(std::numeric_limits<uint32_t>::min())
                        .has_value());
        EXPECT_TRUE(writer.WriteUint32(std::numeric_limits<uint32_t>::max())
                        .has_value());
      },
      [](const ParcelReader& reader) {
        EXPECT_EQ(42u, *reader.ReadUint32());
        EXPECT_EQ(std::numeric_limits<uint32_t>::min(), *reader.ReadUint32());
        EXPECT_EQ(std::numeric_limits<uint32_t>::max(), *reader.ReadUint32());
      });
}

TEST_F(BinderTest, ReadWriteUint64) {
  auto reflector = MakeRefCounted<Reflector>();
  reflector->Reflect(
      [](const ParcelWriter& writer) {
        EXPECT_TRUE(writer.WriteUint64(42).has_value());
        EXPECT_TRUE(writer.WriteUint64(std::numeric_limits<uint64_t>::min())
                        .has_value());
        EXPECT_TRUE(writer.WriteUint64(std::numeric_limits<uint64_t>::max())
                        .has_value());
      },
      [](const ParcelReader& reader) {
        EXPECT_EQ(42u, *reader.ReadUint64());
        EXPECT_EQ(std::numeric_limits<uint64_t>::min(), *reader.ReadUint64());
        EXPECT_EQ(std::numeric_limits<uint64_t>::max(), *reader.ReadUint64());
      });
}

TEST_F(BinderTest, ReadWriteByteArray) {
  auto reflector = MakeRefCounted<Reflector>();
  const uint8_t kPrimeData[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
  reflector->Reflect(
      [&](const ParcelWriter& writer) {
        EXPECT_TRUE(writer.WriteByteArray(kPrimeData).has_value());
      },
      [&](const ParcelReader& reader) {
        std::vector<uint8_t> buffer;
        EXPECT_TRUE(reader
                        .ReadByteArray([&](size_t size) {
                          buffer.resize(size);
                          return buffer.data();
                        })
                        .has_value());
        EXPECT_TRUE(std::equal(buffer.begin(), buffer.end(),
                               std::begin(kPrimeData), std::end(kPrimeData)));
      });
}

TEST_F(BinderTest, ReadWriteEmptyByteArray) {
  auto reflector = MakeRefCounted<Reflector>();
  reflector->Reflect(
      [&](const ParcelWriter& writer) {
        EXPECT_TRUE(writer.WriteByteArray({}).has_value());
      },
      [](const ParcelReader& reader) {
        EXPECT_TRUE(reader
                        .ReadByteArray([](size_t size) -> uint8_t* {
                          // We don't call the allocator for empty arrays.
                          NOTREACHED();
                        })
                        .has_value());
      });
}

TEST_F(BinderTest, ReadWriteFileDescriptor) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  File file(temp_dir.GetPath().AppendASCII("test_file"),
            File::Flags::FLAG_CREATE | File::Flags::FLAG_WRITE);
  auto reflector = MakeRefCounted<Reflector>();
  reflector->Reflect(
      [&](const ParcelWriter& writer) {
        EXPECT_TRUE(
            writer.WriteFileDescriptor(ScopedFD(file.TakePlatformFile()))
                .has_value());
      },
      [](const ParcelReader& reader) {
        ScopedFD fd = *reader.ReadFileDescriptor();
        EXPECT_TRUE(fd.is_valid());
      });
}

class AddInterface {
 public:
  DEFINE_BINDER_CLASS(Class);

  static constexpr transaction_code_t kAdd = 1234;

  class Proxy : public Class::BinderRef {
   public:
    explicit Proxy(BinderRef binder) : Class::BinderRef(std::move(binder)) {}

    int32_t Add(int32_t n) {
      const Parcel sum =
          *Transact(kAdd, [n](const auto& p) { return p.WriteInt32(n); });
      return *sum.reader().ReadInt32();
    }
  };
};

// Test service which adds a fixed offset to any transacted values;
class AddService : public SupportsBinder<AddInterface::Class> {
 public:
  explicit AddService(int32_t offset) : offset_(offset) {}

  void set_destruction_callback(base::OnceClosure callback) {
    destruction_callback_ = std::move(callback);
  }

  void set_binder_destruction_callback(base::RepeatingClosure callback) {
    binder_destruction_callback_ = std::move(callback);
  }

  // SupportsBinder<AddInterface::Class>:
  BinderStatusOr<void> OnBinderTransaction(transaction_code_t code,
                                           const ParcelReader& in,
                                           const ParcelWriter& out) override {
    EXPECT_EQ(AddInterface::kAdd, code);
    return out.WriteInt32(*in.ReadInt32() + offset_);
  }

  void OnBinderDestroyed() override {
    if (binder_destruction_callback_) {
      binder_destruction_callback_.Run();
    }
  }

 private:
  ~AddService() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  const int32_t offset_;

  base::OnceClosure destruction_callback_;
  base::RepeatingClosure binder_destruction_callback_;
};

TEST_F(BinderTest, NullBinderRef) {
  BinderRef ref;
  EXPECT_FALSE(ref);

  TypedBinderRef<AddInterface::Class> typed_ref(std::move(ref));
  EXPECT_FALSE(typed_ref);
}

TEST_F(BinderTest, BasicTransaction) {
  auto add42_service = base::MakeRefCounted<AddService>(42);
  AddInterface::Proxy add42(add42_service->GetBinder());
  EXPECT_EQ(47, add42.Add(5));
}

TEST_F(BinderTest, Lifecycle) {
  auto add42_service = base::MakeRefCounted<AddService>(42);
  AddInterface::Proxy add42(add42_service->GetBinder());

  bool is_destroyed = false;
  base::WaitableEvent destruction;
  add42_service->set_destruction_callback(base::BindLambdaForTesting([&] {
    is_destroyed = true;
    destruction.Signal();
  }));
  add42_service.reset();

  EXPECT_FALSE(is_destroyed);

  EXPECT_EQ(47, add42.Add(5));
  add42.reset();
  destruction.Wait();

  EXPECT_TRUE(is_destroyed);
}

TEST_F(BinderTest, OnBinderDestroyed) {
  auto add5_service = base::MakeRefCounted<AddService>(5);

  bool has_binder = true;
  base::WaitableEvent binder_destruction;
  add5_service->set_binder_destruction_callback(base::BindLambdaForTesting([&] {
    has_binder = false;
    binder_destruction.Signal();
  }));

  AddInterface::Proxy add5(add5_service->GetBinder());
  EXPECT_TRUE(has_binder);
  EXPECT_EQ(12, add5.Add(7));
  add5.reset();
  binder_destruction.Wait();
  EXPECT_FALSE(has_binder);

  binder_destruction.Reset();
  has_binder = true;

  AddInterface::Proxy add5_1(add5_service->GetBinder());
  AddInterface::Proxy add5_2(add5_service->GetBinder());
  EXPECT_EQ(6, add5_1.Add(1));
  EXPECT_EQ(7, add5_2.Add(2));

  add5_1.reset();
  EXPECT_TRUE(has_binder);
  add5_2.reset();
  binder_destruction.Wait();
  EXPECT_FALSE(has_binder);
}

class MultiplyInterface {
 public:
  DEFINE_BINDER_CLASS(Class);

  static constexpr transaction_code_t kMultiply = 5678;

  class Proxy : public Class::BinderRef {
   public:
    explicit Proxy(BinderRef binder) : Class::BinderRef(std::move(binder)) {}

    int32_t Multiply(int32_t n) {
      const Parcel product =
          *Transact(kMultiply, [n](const auto& p) { return p.WriteInt32(n); });
      return *product.reader().ReadInt32();
    }
  };
};

// Test service which multiplies transacted values by a fixed scale.
class MultiplyService : public SupportsBinder<MultiplyInterface::Class> {
 public:
  explicit MultiplyService(int32_t scale) : scale_(scale) {}

  void set_destruction_callback(base::OnceClosure callback) {
    destruction_callback_ = std::move(callback);
  }

  // SupportsBinder<MultiplyInterface::Class>:
  BinderStatusOr<void> OnBinderTransaction(transaction_code_t code,
                                           const ParcelReader& in,
                                           const ParcelWriter& out) override {
    EXPECT_EQ(MultiplyInterface::kMultiply, code);
    return out.WriteInt32(*in.ReadInt32() * scale_);
  }

 private:
  ~MultiplyService() override {
    if (destruction_callback_) {
      std::move(destruction_callback_).Run();
    }
  }

  const int32_t scale_;

  base::OnceClosure destruction_callback_;
};

TEST_F(BinderTest, MultipleInstances) {
  auto add100_service = base::MakeRefCounted<AddService>(100);
  auto add200_service = base::MakeRefCounted<AddService>(200);
  AddInterface::Proxy add100(add100_service->GetBinder());
  AddInterface::Proxy add200(add200_service->GetBinder());
  EXPECT_EQ(105, add100.Add(5));
  EXPECT_EQ(207, add200.Add(7));
}

TEST_F(BinderTest, MultipleClasses) {
  auto add100_service = base::MakeRefCounted<AddService>(100);
  auto multiply7_service = base::MakeRefCounted<MultiplyService>(7);
  AddInterface::Proxy add100(add100_service->GetBinder());
  MultiplyInterface::Proxy multiply7(multiply7_service->GetBinder());
  EXPECT_EQ(105, add100.Add(5));
  EXPECT_EQ(63, multiply7.Multiply(9));
}

class MathInterface {
 public:
  DEFINE_BINDER_CLASS(Class);

  static constexpr transaction_code_t kGetAdd = 1;
  static constexpr transaction_code_t kGetMultiply = 2;

  class Proxy : public Class::BinderRef {
   public:
    explicit Proxy(BinderRef binder) : Class::BinderRef(std::move(binder)) {}

    AddInterface::Proxy GetAdd(int32_t offset) {
      auto reply = *Transact(
          kGetAdd, [offset](const auto& p) { return p.WriteInt32(offset); });
      return AddInterface::Proxy(*reply.reader().ReadBinder());
    }

    MultiplyInterface::Proxy GetMultiply(int32_t scale) {
      auto reply = *Transact(
          kGetMultiply, [scale](const auto& p) { return p.WriteInt32(scale); });
      return MultiplyInterface::Proxy(*reply.reader().ReadBinder());
    }
  };
};

// A service which expects transactions requesting new AddInterface or
// MultiplyInterface binders with a respective offset or scale. Each request
// returns a binder for a bespoke service instance configured accordingly.
class MathService : public SupportsBinder<MathInterface::Class> {
 public:
  // SupportsBinder<MathInterface::Class>:
  BinderStatusOr<void> OnBinderTransaction(transaction_code_t code,
                                           const ParcelReader& in,
                                           const ParcelWriter& out) override {
    ASSIGN_OR_RETURN(const int32_t value, in.ReadInt32());
    switch (code) {
      case MathInterface::kGetAdd: {
        auto service = base::MakeRefCounted<AddService>(value);
        RETURN_IF_ERROR(out.WriteBinder(service->GetBinder()));
        service->set_destruction_callback(MakeNewServiceDestructionCallback());
        break;
      }

      case MathInterface::kGetMultiply: {
        auto service = base::MakeRefCounted<MultiplyService>(value);
        RETURN_IF_ERROR(out.WriteBinder(service->GetBinder()));
        service->set_destruction_callback(MakeNewServiceDestructionCallback());
        break;
      }

      default:
        NOTREACHED();
    }
    return base::ok();
  }

  void WaitForAllServicesToBeDestroyed() { all_services_destroyed_.Wait(); }

 private:
  base::OnceClosure MakeNewServiceDestructionCallback() {
    num_service_instances_.fetch_add(1, std::memory_order_relaxed);
    return base::BindLambdaForTesting([this] {
      if (num_service_instances_.fetch_sub(1, std::memory_order_relaxed) == 1) {
        all_services_destroyed_.Signal();
      }
    });
  }

  ~MathService() override = default;

  std::atomic_int num_service_instances_{0};
  base::WaitableEvent all_services_destroyed_;
};

TEST_F(BinderTest, BindersInTransactions) {
  auto math_service = base::MakeRefCounted<MathService>();
  MathInterface::Proxy math(math_service->GetBinder());

  auto add2 = math.GetAdd(2);
  auto multiply3 = math.GetMultiply(3);
  EXPECT_EQ(8002, add2.Add(8000));
  EXPECT_EQ(27000, multiply3.Multiply(9000));
  add2.reset();
  multiply3.reset();

  math_service->WaitForAllServicesToBeDestroyed();
}

class BinderMultiprocessTest : public BinderTest {
 public:
  template <typename... Binders>
  Process LaunchChild(const char* name, Binders&&... binders) {
    CommandLine cmd = GetMultiProcessTestChildBaseCommandLine();
    LaunchOptions options;
    options.binders =
        std::vector<BinderRef>({std::forward<Binders>(binders)...});
    return SpawnMultiProcessTestChild(name, cmd, options);
  }

  bool JoinChild(const Process& child) {
    int child_exit_code = -1;
    WaitForMultiprocessTestChildExit(child, TestTimeouts::action_timeout(),
                                     &child_exit_code);
    return child_exit_code == 0;
  }
};

// Helper to define child process logic such that EXPECT* macro failures will
// impact the process exit code and thereby percolate up to the parent test
// process when joining the child.
#define BINDER_TEST_CHILD_MAIN(name)              \
  class name##_Child {                            \
   public:                                        \
    void Run();                                   \
  };                                              \
  MULTIPROCESS_TEST_MAIN(name) {                  \
    name##_Child().Run();                         \
    return ::testing::Test::HasFailure() ? 1 : 0; \
  }                                               \
  void name##_Child::Run()

TEST_F(BinderMultiprocessTest, PassBinder) {
  auto add42 = base::MakeRefCounted<AddService>(42);
  Process child = LaunchChild("PassBinder_Child", add42->GetBinder());
  EXPECT_TRUE(JoinChild(child));
}

BINDER_TEST_CHILD_MAIN(PassBinder_Child) {
  AddInterface::Proxy add42(TakeBinderFromParent(0));
  EXPECT_EQ(47, add42.Add(5));
}

TEST_F(BinderMultiprocessTest, PassMultipleBinders) {
  auto add100 = base::MakeRefCounted<AddService>(100);
  auto multiply7 = base::MakeRefCounted<MultiplyService>(7);
  Process child = LaunchChild("PassMultipleBinders_Child", add100->GetBinder(),
                              multiply7->GetBinder());
  EXPECT_TRUE(JoinChild(child));
}

BINDER_TEST_CHILD_MAIN(PassMultipleBinders_Child) {
  AddInterface::Proxy add100(TakeBinderFromParent(0));
  MultiplyInterface::Proxy multiply7(TakeBinderFromParent(1));
  EXPECT_EQ(105, add100.Add(5));
  EXPECT_EQ(63, multiply7.Multiply(9));
}

TEST_F(BinderMultiprocessTest, BindersInTransactions) {
  auto math = base::MakeRefCounted<MathService>();
  Process child = LaunchChild("BindersInTransactions_Child", math->GetBinder());
  math->WaitForAllServicesToBeDestroyed();
  EXPECT_TRUE(JoinChild(child));
}

BINDER_TEST_CHILD_MAIN(BindersInTransactions_Child) {
  MathInterface::Proxy math(TakeBinderFromParent(0));
  auto add2 = math.GetAdd(2);
  auto multiply3 = math.GetMultiply(3);
  EXPECT_EQ(8002, add2.Add(8000));
  EXPECT_EQ(27000, multiply3.Multiply(9000));
}

TEST_F(BinderMultiprocessTest, PassedBinderLifetime) {
  auto service = base::MakeRefCounted<AddService>(15);
  bool is_destroyed = false;
  base::WaitableEvent destruction;
  service->set_destruction_callback(base::BindLambdaForTesting([&] {
    is_destroyed = true;
    destruction.Signal();
  }));

  auto binder = service->GetBinder();
  service.reset();

  EXPECT_FALSE(is_destroyed);
  Process child = LaunchChild("PassedBinderLifetime_Child", std::move(binder));
  EXPECT_TRUE(JoinChild(child));
  destruction.Wait();
  EXPECT_TRUE(is_destroyed);
}

BINDER_TEST_CHILD_MAIN(PassedBinderLifetime_Child) {
  AddInterface::Proxy add15(TakeBinderFromParent(0));
  EXPECT_EQ(18, add15.Add(3));
}

DEFINE_BINDER_CLASS(BinderSinkClass);

class BinderSink : public SupportsBinder<BinderSinkClass> {
 public:
  using Callback = RepeatingCallback<void(BinderRef)>;

  explicit BinderSink(BinderMultiprocessTest& test) : test_(test) {}

  template <typename Fn>
  Process LaunchChildAndWaitForBinder(const char* child_name, Fn fn) {
    callback_ = BindLambdaForTesting(fn);
    Process process = test_->LaunchChild(child_name, GetBinder());
    called_.Wait();
    return process;
  }

  void WaitForDisconnect() { disconnected_.Wait(); }

  // SupportsBinder<BinderSinkClass>:
  BinderStatusOr<void> OnBinderTransaction(transaction_code_t code,
                                           const ParcelReader& reader,
                                           const ParcelWriter& out) override {
    ASSIGN_OR_RETURN(auto binder, reader.ReadBinder());
    callback_.Run(std::move(binder));
    called_.Signal();
    return base::ok();
  }

  void OnBinderDestroyed() override { disconnected_.Signal(); }

 private:
  ~BinderSink() override = default;

  raw_ref<BinderMultiprocessTest> test_;
  BinderRef child_binder_;
  Callback callback_;
  WaitableEvent called_;
  WaitableEvent disconnected_;
};

TEST_F(BinderMultiprocessTest, AssociateValid) {
  auto sink = base::MakeRefCounted<BinderSink>(*this);
  Process child = sink->LaunchChildAndWaitForBinder(
      "Associate_Child", [](BinderRef binder) {
        EXPECT_TRUE(
            binder.AssociateWithClass(AddInterface::Class::GetBinderClass()));
      });
  EXPECT_TRUE(JoinChild(child));
}

// (crbug.com/365998251): Builder failing this unittest.
TEST_F(BinderMultiprocessTest, DISABLED_AssociateInvalid) {
  auto sink = base::MakeRefCounted<BinderSink>(*this);
  Process child = sink->LaunchChildAndWaitForBinder(
      "Associate_Child", [](BinderRef binder) {
        EXPECT_FALSE(binder.AssociateWithClass(
            MultiplyInterface::Class::GetBinderClass()));
      });
  EXPECT_TRUE(JoinChild(child));
}

BINDER_TEST_CHILD_MAIN(Associate_Child) {
  auto sink = BinderSinkClass::AdoptBinderRef(TakeBinderFromParent(0));
  auto service = base::MakeRefCounted<AddService>(15);
  std::ignore = sink.Transact(42, [&service](ParcelWriter in) {
    return in.WriteBinder(service->GetBinder());
  });
}

TEST_F(BinderMultiprocessTest, AssociateDestroyed) {
  auto sink = base::MakeRefCounted<BinderSink>(*this);
  BinderRef child_binder;
  Process child = sink->LaunchChildAndWaitForBinder(
      "AssociateDestroyed_Child",
      [&](BinderRef binder) { child_binder = std::move(binder); });
  sink->WaitForDisconnect();

  // Though the class would be correct, association should fail because the
  // child process is already dead and the class descriptor can't be validated.
  EXPECT_FALSE(
      child_binder.AssociateWithClass(AddInterface::Class::GetBinderClass()));
}

BINDER_TEST_CHILD_MAIN(AssociateDestroyed_Child) {
  auto sink = BinderSinkClass::AdoptBinderRef(TakeBinderFromParent(0));
  auto service = base::MakeRefCounted<AddService>(15);
  std::ignore = sink.Transact(42, [&](ParcelWriter in) {
    return in.WriteBinder(service->GetBinder());
  });
  Process::TerminateCurrentProcessImmediately(0);
}

}  // namespace
}  // namespace base::android
