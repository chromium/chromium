// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/traced_value_support.h"

#include <optional>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/test/traced_value_test_support.h"

namespace base {
namespace trace_event {

namespace {

struct RefCountedData : RefCounted<RefCountedData> {
 public:
  explicit RefCountedData(std::string data) : data_(data) {}

  void WriteIntoTrace(perfetto::TracedValue context) const {
    std::move(context).WriteString(data_);
  }

 private:
  ~RefCountedData() = default;
  friend class RefCounted<RefCountedData>;

  std::string data_;
};

struct WeakData {
 public:
  explicit WeakData(std::string data) : data_(data) {}

  void WriteIntoTrace(perfetto::TracedValue context) const {
    std::move(context).WriteString(data_);
  }

  base::WeakPtr<const WeakData> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::string data_;

  base::WeakPtrFactory<WeakData> weak_ptr_factory_{this};
};

}  // namespace

TEST(TracedValueSupportTest, ScopedRefPtr) {
  EXPECT_EQ(
      perfetto::TracedValueToString(scoped_refptr<RefCountedData>(nullptr)),
      "0x0");
  scoped_refptr<RefCountedData> data =
      base::MakeRefCounted<RefCountedData>("refcounted");
  EXPECT_EQ(perfetto::TracedValueToString(data), "refcounted");
}

TEST(TracedValueSupportTest, Optional) {
  EXPECT_EQ(perfetto::TracedValueToString(std::optional<int>()), "0x0");
  EXPECT_EQ(perfetto::TracedValueToString(std::optional<const int>(42)), "42");
}

TEST(TracedValueSupportTest, WeakPtr) {
  std::unique_ptr<WeakData> data = std::make_unique<WeakData>("weak");
  base::WeakPtr<const WeakData> weak_ptr = data->GetWeakPtr();
  EXPECT_EQ(perfetto::TracedValueToString(weak_ptr), "weak");
  data.reset();
  EXPECT_EQ(perfetto::TracedValueToString(weak_ptr), "0x0");
}

TEST(TracedValueSupportTest, Time) {
  EXPECT_EQ(perfetto::TracedValueToString(base::Microseconds(42)), "42");
  EXPECT_EQ(
      perfetto::TracedValueToString(base::Time() + base::Microseconds(42)),
      "42");
  EXPECT_EQ(
      perfetto::TracedValueToString(base::TimeTicks() + base::Microseconds(42)),
      "42");
}

TEST(TracedValueSupportTest, UnguessableToken) {
  auto token = UnguessableToken::Create();
  EXPECT_EQ(perfetto::TracedValueToString(token), token.ToString());
}

TEST(TracedValueSupportTest, UTF16String) {
  EXPECT_EQ(perfetto::TracedValueToString(u"utf-16"), "utf-16");
  EXPECT_EQ(
      perfetto::TracedValueToString(static_cast<const char16_t*>(u"utf-16")),
      "utf-16");
  EXPECT_EQ(perfetto::TracedValueToString(std::u16string(u"utf-16")), "utf-16");
}

TEST(TracedValueSupportTest, WideString) {
  EXPECT_EQ(perfetto::TracedValueToString(L"wide"), "wide");
  EXPECT_EQ(perfetto::TracedValueToString(static_cast<const wchar_t*>(L"wide")),
            "wide");
  EXPECT_EQ(perfetto::TracedValueToString(std::wstring(L"wide")), "wide");
}

TEST(TracedValueSupportTest, StdString) {
  EXPECT_EQ(perfetto::TracedValueToString(std::string_view("string")),
            "string");
  EXPECT_EQ(perfetto::TracedValueToString(std::u16string_view(u"utf-16")),
            "utf-16");
  EXPECT_EQ(perfetto::TracedValueToString(std::wstring_view(L"wide")), "wide");
}

TEST(TracedValueSupportTest, RawPtr) {
  // Serialise nullptr.
  EXPECT_EQ(perfetto::TracedValueToString(raw_ptr<int>()), "0x0");

  {
    // If the pointer is non-null, its dereferenced value will be serialised.
    int value = 42;
    raw_ptr<int> value_simple(&value);
    raw_ptr<int, AllowPtrArithmetic> value_with_traits(&value);

    EXPECT_EQ(perfetto::TracedValueToString(value), "42");
    EXPECT_EQ(perfetto::TracedValueToString(value_with_traits), "42");
  }

  struct WithTraceSupport {
    void WriteIntoTrace(perfetto::TracedValue ctx) const {
      std::move(ctx).WriteString("result");
    }
  };

  {
    WithTraceSupport value;
    raw_ptr<WithTraceSupport> value_simple(&value);
    raw_ptr<WithTraceSupport, AllowPtrArithmetic> value_with_traits(&value);

    EXPECT_EQ(perfetto::TracedValueToString(value_simple), "result");
    EXPECT_EQ(perfetto::TracedValueToString(value_with_traits), "result");
  }
}

TEST(TracedValueSupportTest, RawRef) {
  {
    int value = 42;
    raw_ref<int> value_simple(value);
    raw_ref<int, AllowPtrArithmetic> value_with_traits(value);

    EXPECT_EQ(perfetto::TracedValueToString(value), "42");
    EXPECT_EQ(perfetto::TracedValueToString(value_with_traits), "42");
  }

  struct WithTraceSupport {
    void WriteIntoTrace(perfetto::TracedValue ctx) const {
      std::move(ctx).WriteString("result");
    }
  };

  {
    WithTraceSupport value;
    raw_ref<WithTraceSupport> value_simple(value);
    raw_ref<WithTraceSupport, AllowPtrArithmetic> value_with_traits(value);

    EXPECT_EQ(perfetto::TracedValueToString(value_simple), "result");
    EXPECT_EQ(perfetto::TracedValueToString(value_with_traits), "result");
  }
}

}  // namespace trace_event
}  // namespace base
