// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_arguments.h"

#include <gtest/gtest.h>
#include <limits>
#include <string>

namespace base {
namespace trace_event {

namespace {

// Simple convertable that holds a string to append to the trace,
// and can also write to a boolean flag on destruction.
class MyConvertable : public ConvertableToTraceFormat {
 public:
  MyConvertable(const char* text, bool* destroy_flag = nullptr)
      : text_(text), destroy_flag_(destroy_flag) {}
  ~MyConvertable() override {
    if (destroy_flag_)
      *destroy_flag_ = true;
  }
  void AppendAsTraceFormat(std::string* out) const override { *out += text_; }
  const char* text() const { return text_; }

 private:
  const char* text_;
  bool* destroy_flag_;
};

}  // namespace

TEST(TraceArguments, StringStorageDefaultConstruction) {
  StringStorage storage;
  EXPECT_TRUE(storage.empty());
  EXPECT_FALSE(storage.data());
  EXPECT_EQ(0U, storage.size());
}

TEST(TraceArguments, StringStorageConstructionWithSize) {
  const size_t kSize = 128;
  StringStorage storage(kSize);
  EXPECT_FALSE(storage.empty());
  EXPECT_TRUE(storage.data());
  EXPECT_EQ(kSize, storage.size());
  EXPECT_EQ(storage.data(), storage.begin());
  EXPECT_EQ(storage.data() + kSize, storage.end());
}

TEST(TraceArguments, StringStorageReset) {
  StringStorage storage(128);
  EXPECT_FALSE(storage.empty());

  storage.Reset();
  EXPECT_TRUE(storage.empty());
  EXPECT_FALSE(storage.data());
  EXPECT_EQ(0u, storage.size());
}

TEST(TraceArguments, StringStorageResetWithSize) {
  StringStorage storage;
  EXPECT_TRUE(storage.empty());

  const size_t kSize = 128;
  storage.Reset(kSize);
  EXPECT_FALSE(storage.empty());
  EXPECT_TRUE(storage.data());
  EXPECT_EQ(kSize, storage.size());
  EXPECT_EQ(storage.data(), storage.begin());
  EXPECT_EQ(storage.data() + kSize, storage.end());
}

TEST(TraceArguments, StringStorageEstimateTraceMemoryOverhead) {
  StringStorage storage;
  EXPECT_EQ(0u, storage.EstimateTraceMemoryOverhead());

  const size_t kSize = 128;
  storage.Reset(kSize);
  EXPECT_EQ(sizeof(size_t) + kSize, storage.EstimateTraceMemoryOverhead());
}

static void CheckJSONFor(TraceValue v, char type, const char* expected) {
  std::string out;
  v.AppendAsJSON(type, &out);
  EXPECT_STREQ(expected, out.c_str());
}

static void CheckStringFor(TraceValue v, char type, const char* expected) {
  std::string out;
  v.AppendAsString(type, &out);
  EXPECT_STREQ(expected, out.c_str());
}

TEST(TraceArguments, TraceValueAppend) {
  TraceValue v;

  v.Init(-1024);
  CheckJSONFor(v, TRACE_VALUE_TYPE_INT, "-1024");
  CheckStringFor(v, TRACE_VALUE_TYPE_INT, "-1024");
  v.Init(1024ULL);
  CheckJSONFor(v, TRACE_VALUE_TYPE_UINT, "1024");
  CheckStringFor(v, TRACE_VALUE_TYPE_UINT, "1024");
  v.Init(3.1415926535);
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "3.1415926535");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "3.1415926535");
  v.Init(2.0);
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "2.0");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "2.0");
  v.Init(0.5);
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "0.5");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "0.5");
  v.Init(-0.5);
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "-0.5");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "-0.5");
  v.Init(std::numeric_limits<double>::quiet_NaN());
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "\"NaN\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "NaN");
  v.Init(std::numeric_limits<double>::quiet_NaN());
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "\"NaN\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "NaN");
  v.Init(std::numeric_limits<double>::infinity());
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "\"Infinity\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "Infinity");
  v.Init(-std::numeric_limits<double>::infinity());
  CheckJSONFor(v, TRACE_VALUE_TYPE_DOUBLE, "\"-Infinity\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_DOUBLE, "-Infinity");
  v.Init(true);
  CheckJSONFor(v, TRACE_VALUE_TYPE_BOOL, "true");
  CheckStringFor(v, TRACE_VALUE_TYPE_BOOL, "true");
  v.Init(false);
  CheckJSONFor(v, TRACE_VALUE_TYPE_BOOL, "false");
  CheckStringFor(v, TRACE_VALUE_TYPE_BOOL, "false");
  v.Init("Some \"nice\" String");
  CheckJSONFor(v, TRACE_VALUE_TYPE_STRING, "\"Some \\\"nice\\\" String\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_STRING, "Some \"nice\" String");
  CheckJSONFor(v, TRACE_VALUE_TYPE_COPY_STRING, "\"Some \\\"nice\\\" String\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_COPY_STRING, "Some \"nice\" String");

  int* p = nullptr;
  v.Init(p);
  CheckJSONFor(v, TRACE_VALUE_TYPE_POINTER, "\"0x0\"");
  CheckStringFor(v, TRACE_VALUE_TYPE_POINTER, "0x0");

  const char kText[] = "Hello World";
  bool destroy_flag = false;
  TraceArguments args("arg1",
                      std::make_unique<MyConvertable>(kText, &destroy_flag));

  CheckJSONFor(std::move(args.values()[0]), args.types()[0], kText);
  CheckStringFor(std::move(args.values()[0]), args.types()[0], kText);
}

TEST(TraceArguments, DefaultConstruction) {
  TraceArguments args;
  EXPECT_EQ(0U, args.size());
}

TEST(TraceArguments, ConstructorSingleInteger) {
  TraceArguments args("foo_int", int(10));
  EXPECT_EQ(1U, args.size());
  EXPECT_EQ(TRACE_VALUE_TYPE_INT, args.types()[0]);
  EXPECT_STREQ("foo_int", args.names()[0]);
  EXPECT_EQ(10, args.values()[0].as_int);
}

TEST(TraceArguments, ConstructorSingleFloat) {
  TraceArguments args("foo_pi", float(3.1415));
  double expected = float(3.1415);
  EXPECT_EQ(1U, args.size());
  EXPECT_EQ(TRACE_VALUE_TYPE_DOUBLE, args.types()[0]);
  EXPECT_STREQ("foo_pi", args.names()[0]);
  EXPECT_EQ(expected, args.values()[0].as_double);
}

TEST(TraceArguments, ConstructorSingleNoCopyString) {
  const char kText[] = "Persistent string";
  TraceArguments args("foo_cstring", kText);
  EXPECT_EQ(1U, args.size());
  EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args.types()[0]);
  EXPECT_STREQ("foo_cstring", args.names()[0]);
  EXPECT_EQ(kText, args.values()[0].as_string);
}

TEST(TraceArguments, ConstructorSingleStdString) {
  std::string text = "Non-persistent string";
  TraceArguments args("foo_stdstring", text);
  EXPECT_EQ(1U, args.size());
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[0]);
  EXPECT_STREQ("foo_stdstring", args.names()[0]);
  EXPECT_EQ(text.c_str(), args.values()[0].as_string);
}

TEST(TraceArguments, ConstructorSingleTraceStringWithCopy) {
  const char kText[] = "Persistent string #2";
  TraceArguments args("foo_tracestring", TraceStringWithCopy(kText));
  EXPECT_EQ(1U, args.size());
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[0]);
  EXPECT_STREQ("foo_tracestring", args.names()[0]);
  EXPECT_EQ(kText, args.values()[0].as_string);
}

TEST(TraceArguments, ConstructorSinglePointer) {
  bool destroy_flag = false;
  {
    // Simple class that can set a boolean flag on destruction.
    class Foo {
     public:
      Foo(bool* destroy_flag) : destroy_flag_(destroy_flag) {}
      ~Foo() {
        if (destroy_flag_)
          *destroy_flag_ = true;
      }

     private:
      bool* destroy_flag_;
    };
    auto foo = std::make_unique<Foo>(&destroy_flag);
    EXPECT_FALSE(destroy_flag);
    // This test also verifies that the object is not destroyed by the
    // TraceArguments destructor. This should only be possible for
    // TRACE_VALUE_TYPE_CONVERTABLE instances.
    {
      TraceArguments args("foo_pointer", foo.get());
      EXPECT_EQ(1U, args.size());
      EXPECT_EQ(TRACE_VALUE_TYPE_POINTER, args.types()[0]);
      EXPECT_STREQ("foo_pointer", args.names()[0]);
      EXPECT_EQ(foo.get(), args.values()[0].as_pointer);
      EXPECT_FALSE(destroy_flag);
    }  // Calls TraceArguments destructor.
    EXPECT_FALSE(destroy_flag);
  }  // Calls Foo destructor.
  EXPECT_TRUE(destroy_flag);
}

TEST(TraceArguments, ConstructorSingleConvertable) {
  bool destroy_flag = false;
  const char kText[] = "Text for MyConvertable instance";
  MyConvertable* ptr = new MyConvertable(kText, &destroy_flag);

  // This test also verifies that the MyConvertable instance is properly
  // destroyed when the TraceArguments destructor is called.
  EXPECT_FALSE(destroy_flag);
  {
    TraceArguments args("foo_convertable", std::unique_ptr<MyConvertable>(ptr));
    EXPECT_EQ(1U, args.size());
    EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args.types()[0]);
    EXPECT_STREQ("foo_convertable", args.names()[0]);
    EXPECT_EQ(ptr, args.values()[0].as_convertable);
    EXPECT_FALSE(destroy_flag);
  }  // Calls TraceArguments destructor.
  EXPECT_TRUE(destroy_flag);
}

TEST(TraceArguments, ConstructorWithTwoArguments) {
  const char kText1[] = "First argument";
  const char kText2[] = "Second argument";
  bool destroy_flag = false;

  {
    MyConvertable* ptr = new MyConvertable(kText2, &destroy_flag);
    TraceArguments args1("foo_arg1_cstring", kText1, "foo_arg2_convertable",
                         std::unique_ptr<MyConvertable>(ptr));
    EXPECT_EQ(2U, args1.size());
    EXPECT_STREQ("foo_arg1_cstring", args1.names()[0]);
    EXPECT_STREQ("foo_arg2_convertable", args1.names()[1]);
    EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args1.types()[0]);
    EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args1.types()[1]);
    EXPECT_EQ(kText1, args1.values()[0].as_string);
    EXPECT_EQ(ptr, args1.values()[1].as_convertable);
    EXPECT_FALSE(destroy_flag);
  }  // calls |args1| destructor. Should delete |ptr|.
  EXPECT_TRUE(destroy_flag);
}

TEST(TraceArguments, ConstructorLegacyNoConvertables) {
  const char* const kNames[3] = {"legacy_arg1", "legacy_arg2", "legacy_arg3"};
  const unsigned char kTypes[3] = {
      TRACE_VALUE_TYPE_INT,
      TRACE_VALUE_TYPE_STRING,
      TRACE_VALUE_TYPE_POINTER,
  };
  static const char kText[] = "Some text";
  const unsigned long long kValues[3] = {
      1000042ULL,
      reinterpret_cast<unsigned long long>(kText),
      reinterpret_cast<unsigned long long>(kText + 2),
  };
  TraceArguments args(3, kNames, kTypes, kValues);
  // Check that only the first kMaxSize arguments are taken!
  EXPECT_EQ(2U, args.size());
  EXPECT_STREQ(kNames[0], args.names()[0]);
  EXPECT_STREQ(kNames[1], args.names()[1]);
  EXPECT_EQ(TRACE_VALUE_TYPE_INT, args.types()[0]);
  EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args.types()[1]);
  EXPECT_EQ(kValues[0], args.values()[0].as_uint);
  EXPECT_EQ(kText, args.values()[1].as_string);
}

TEST(TraceArguments, ConstructorLegacyWithConvertables) {
  const char* const kNames[3] = {"legacy_arg1", "legacy_arg2", "legacy_arg3"};
  const unsigned char kTypes[3] = {
      TRACE_VALUE_TYPE_CONVERTABLE,
      TRACE_VALUE_TYPE_CONVERTABLE,
      TRACE_VALUE_TYPE_CONVERTABLE,
  };
  std::unique_ptr<MyConvertable> convertables[3] = {
      std::make_unique<MyConvertable>("First one"),
      std::make_unique<MyConvertable>("Second one"),
      std::make_unique<MyConvertable>("Third one"),
  };
  TraceArguments args(3, kNames, kTypes, nullptr, convertables);
  // Check that only the first kMaxSize arguments are taken!
  EXPECT_EQ(2U, args.size());
  EXPECT_STREQ(kNames[0], args.names()[0]);
  EXPECT_STREQ(kNames[1], args.names()[1]);
  EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args.types()[0]);
  EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args.types()[1]);
  // Check that only the first two items were moved to |args|.
  EXPECT_FALSE(convertables[0].get());
  EXPECT_FALSE(convertables[1].get());
  EXPECT_TRUE(convertables[2].get());
}

TEST(TraceArguments, MoveConstruction) {
  const char kText1[] = "First argument";
  const char kText2[] = "Second argument";
  bool destroy_flag = false;

  {
    MyConvertable* ptr = new MyConvertable(kText2, &destroy_flag);
    TraceArguments args1("foo_arg1_cstring", kText1, "foo_arg2_convertable",
                         std::unique_ptr<MyConvertable>(ptr));
    EXPECT_EQ(2U, args1.size());
    EXPECT_STREQ("foo_arg1_cstring", args1.names()[0]);
    EXPECT_STREQ("foo_arg2_convertable", args1.names()[1]);
    EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args1.types()[0]);
    EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args1.types()[1]);
    EXPECT_EQ(kText1, args1.values()[0].as_string);
    EXPECT_EQ(ptr, args1.values()[1].as_convertable);

    {
      TraceArguments args2(std::move(args1));
      EXPECT_FALSE(destroy_flag);

      // |args1| is now empty.
      EXPECT_EQ(0U, args1.size());

      // Check that everything was transferred to |args2|.
      EXPECT_EQ(2U, args2.size());
      EXPECT_STREQ("foo_arg1_cstring", args2.names()[0]);
      EXPECT_STREQ("foo_arg2_convertable", args2.names()[1]);
      EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args2.types()[0]);
      EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args2.types()[1]);
      EXPECT_EQ(kText1, args2.values()[0].as_string);
      EXPECT_EQ(ptr, args2.values()[1].as_convertable);
    }  // Calls |args2| destructor. Should delete |ptr|.
    EXPECT_TRUE(destroy_flag);
    destroy_flag = false;
  }  // Calls |args1| destructor. Should not delete |ptr|.
  EXPECT_FALSE(destroy_flag);
}

TEST(TraceArguments, MoveAssignment) {
  const char kText1[] = "First argument";
  const char kText2[] = "Second argument";
  bool destroy_flag = false;

  {
    MyConvertable* ptr = new MyConvertable(kText2, &destroy_flag);
    TraceArguments args1("foo_arg1_cstring", kText1, "foo_arg2_convertable",
                         std::unique_ptr<MyConvertable>(ptr));
    EXPECT_EQ(2U, args1.size());
    EXPECT_STREQ("foo_arg1_cstring", args1.names()[0]);
    EXPECT_STREQ("foo_arg2_convertable", args1.names()[1]);
    EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args1.types()[0]);
    EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args1.types()[1]);
    EXPECT_EQ(kText1, args1.values()[0].as_string);
    EXPECT_EQ(ptr, args1.values()[1].as_convertable);

    {
      TraceArguments args2;

      args2 = std::move(args1);
      EXPECT_FALSE(destroy_flag);

      // |args1| is now empty.
      EXPECT_EQ(0U, args1.size());

      // Check that everything was transferred to |args2|.
      EXPECT_EQ(2U, args2.size());
      EXPECT_STREQ("foo_arg1_cstring", args2.names()[0]);
      EXPECT_STREQ("foo_arg2_convertable", args2.names()[1]);
      EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args2.types()[0]);
      EXPECT_EQ(TRACE_VALUE_TYPE_CONVERTABLE, args2.types()[1]);
      EXPECT_EQ(kText1, args2.values()[0].as_string);
      EXPECT_EQ(ptr, args2.values()[1].as_convertable);
    }  // Calls |args2| destructor. Should delete |ptr|.
    EXPECT_TRUE(destroy_flag);
    destroy_flag = false;
  }  // Calls |args1| destructor. Should not delete |ptr|.
  EXPECT_FALSE(destroy_flag);
}

TEST(TraceArguments, Reset) {
  bool destroy_flag = false;
  {
    TraceArguments args(
        "foo_arg1", "Hello", "foo_arg2",
        std::make_unique<MyConvertable>("World", &destroy_flag));

    EXPECT_EQ(2U, args.size());
    EXPECT_FALSE(destroy_flag);
    args.Reset();
    EXPECT_EQ(0U, args.size());
    EXPECT_TRUE(destroy_flag);
    destroy_flag = false;
  }  // Calls |args| destructor. Should not delete twice.
  EXPECT_FALSE(destroy_flag);
}

TEST(TraceArguments, CopyStringsTo_NoStrings) {
  StringStorage storage;

  TraceArguments args("arg1", 10, "arg2", 42);
  args.CopyStringsTo(&storage, false, nullptr, nullptr);
  EXPECT_TRUE(storage.empty());
  EXPECT_EQ(0U, storage.size());
}

TEST(TraceArguments, CopyStringsTo_OnlyArgs) {
  StringStorage storage;

  TraceArguments args("arg1", TraceStringWithCopy("Hello"), "arg2",
                      TraceStringWithCopy("World"));

  const char kExtra1[] = "extra1";
  const char kExtra2[] = "extra2";
  const char* extra1 = kExtra1;
  const char* extra2 = kExtra2;

  // Types should be copyable strings.
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[0]);
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[1]);

  args.CopyStringsTo(&storage, false, &extra1, &extra2);

  // Storage should be allocated.
  EXPECT_TRUE(storage.data());
  EXPECT_NE(0U, storage.size());

  // Types should not be changed.
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[0]);
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[1]);

  // names should not be copied.
  EXPECT_FALSE(storage.Contains(args.names()[0]));
  EXPECT_FALSE(storage.Contains(args.names()[1]));
  EXPECT_STREQ("arg1", args.names()[0]);
  EXPECT_STREQ("arg2", args.names()[1]);

  // strings should be copied.
  EXPECT_TRUE(storage.Contains(args.values()[0].as_string));
  EXPECT_TRUE(storage.Contains(args.values()[1].as_string));
  EXPECT_STREQ("Hello", args.values()[0].as_string);
  EXPECT_STREQ("World", args.values()[1].as_string);

  // |extra1| and |extra2| should not be copied.
  EXPECT_EQ(kExtra1, extra1);
  EXPECT_EQ(kExtra2, extra2);
}

TEST(TraceArguments, CopyStringsTo_Everything) {
  StringStorage storage;

  TraceArguments args("arg1", "Hello", "arg2", "World");
  const char kExtra1[] = "extra1";
  const char kExtra2[] = "extra2";
  const char* extra1 = kExtra1;
  const char* extra2 = kExtra2;

  // Types should be normal strings.
  EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args.types()[0]);
  EXPECT_EQ(TRACE_VALUE_TYPE_STRING, args.types()[1]);

  args.CopyStringsTo(&storage, true, &extra1, &extra2);

  // Storage should be allocated.
  EXPECT_TRUE(storage.data());
  EXPECT_NE(0U, storage.size());

  // Types should be changed to copyable strings.
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[0]);
  EXPECT_EQ(TRACE_VALUE_TYPE_COPY_STRING, args.types()[1]);

  // names should be copied.
  EXPECT_TRUE(storage.Contains(args.names()[0]));
  EXPECT_TRUE(storage.Contains(args.names()[1]));
  EXPECT_STREQ("arg1", args.names()[0]);
  EXPECT_STREQ("arg2", args.names()[1]);

  // strings should be copied.
  EXPECT_TRUE(storage.Contains(args.values()[0].as_string));
  EXPECT_TRUE(storage.Contains(args.values()[1].as_string));
  EXPECT_STREQ("Hello", args.values()[0].as_string);
  EXPECT_STREQ("World", args.values()[1].as_string);

  // |extra1| and |extra2| should be copied.
  EXPECT_NE(kExtra1, extra1);
  EXPECT_NE(kExtra2, extra2);
  EXPECT_TRUE(storage.Contains(extra1));
  EXPECT_TRUE(storage.Contains(extra2));
  EXPECT_STREQ(kExtra1, extra1);
  EXPECT_STREQ(kExtra2, extra2);
}

}  // namespace trace_event
}  // namespace base
