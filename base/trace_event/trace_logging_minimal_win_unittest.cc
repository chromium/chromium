// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_logging_minimal_win.h"

#include <gtest/gtest.h>

#include "base/functional/callback_helpers.h"

// Test fixture for TlmProvider
class TlmProviderTest : public ::testing::Test {
 protected:
  TlmProviderTest() = default;

  void MockOSEventRegister() {
    TlmProvider::StaticEnableCallback(nullptr, 1, 1, 0, 0, nullptr, &provider_);
  }

  TlmProvider provider_;
};

// Test default constructor
TEST_F(TlmProviderTest, DefaultConstructor) {
  TlmProvider default_provider;
  EXPECT_FALSE(default_provider.IsEnabled());
}

// Test parameterized constructor
TEST_F(TlmProviderTest, ParameterizedConstructor) {
  EXPECT_FALSE(provider_.IsEnabled());
}

// Test Register method
TEST_F(TlmProviderTest, Register) {
  ULONG result = provider_.Register("TestProvider", GUID(), base::DoNothing());
  EXPECT_EQ(result, 0UL);
}

// Test Unregister method
TEST_F(TlmProviderTest, Unregister) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  provider_.Unregister();
  EXPECT_FALSE(provider_.IsEnabled());
}

// Test IsEnabled method
TEST_F(TlmProviderTest, IsEnabled) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  EXPECT_FALSE(provider_.IsEnabled());
  MockOSEventRegister();
  EXPECT_TRUE(provider_.IsEnabled());
}

// Test WriteEvent method
TEST_F(TlmProviderTest, WriteEvent) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  MockOSEventRegister();

  EXPECT_TRUE(provider_.IsEnabled());
  EVENT_DESCRIPTOR event_descriptor = {};
  ULONG result = provider_.WriteEvent("TestEvent", event_descriptor);
  EXPECT_EQ(result, 0UL);
}

// Test WriteEvent with TlmMbcsStringField
TEST_F(TlmProviderTest, WriteEventWithMbcsStringField) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  MockOSEventRegister();

  EXPECT_TRUE(provider_.IsEnabled());
  EVENT_DESCRIPTOR event_descriptor = {};
  TlmMbcsStringField field("TestField", "TestValue");
  ULONG result = provider_.WriteEvent("TestEvent", event_descriptor, field);
  EXPECT_EQ(result, 0UL);
}

// Test WriteEvent with TlmUtf8StringField
TEST_F(TlmProviderTest, WriteEventWithUtf8StringField) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  MockOSEventRegister();

  EXPECT_TRUE(provider_.IsEnabled());
  EVENT_DESCRIPTOR event_descriptor = {};
  TlmUtf8StringField field("TestField", "TestValue");
  ULONG result = provider_.WriteEvent("TestEvent", event_descriptor, field);
  EXPECT_EQ(result, 0UL);
}

// Test WriteEvent with TlmInt64Field
TEST_F(TlmProviderTest, WriteEventWithInt64Field) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  MockOSEventRegister();

  EXPECT_TRUE(provider_.IsEnabled());
  EVENT_DESCRIPTOR event_descriptor = {};
  TlmInt64Field field("TestField", 1234567890);
  ULONG result = provider_.WriteEvent("TestEvent", event_descriptor, field);
  EXPECT_EQ(result, 0UL);
}

// Test WriteEvent with TlmUInt64Field
TEST_F(TlmProviderTest, WriteEventWithUInt64Field) {
  provider_.Register("TestProvider", GUID(), base::DoNothing());
  MockOSEventRegister();

  EXPECT_TRUE(provider_.IsEnabled());
  EVENT_DESCRIPTOR event_descriptor = {};
  TlmUInt64Field field("TestField", 1234567890);
  ULONG result = provider_.WriteEvent("TestEvent", event_descriptor, field);
  EXPECT_EQ(result, 0UL);
}

// Test fixture for TlmInt64Field
class TlmStringView final : public TlmFieldBase {
 public:
  TlmStringView(std::string_view name) : TlmFieldBase(name) {}
};

class TlmChar final : public TlmFieldBase {
 public:
  TlmChar(const char* name) : TlmFieldBase(name) {}
};

class TlmFieldBaseTest : public ::testing::Test {};

// Test the constructor and Value() method
TEST_F(TlmFieldBaseTest, CharConstructorAndValue) {
  char name[] = {'T', 'e', 's', 't', 'F', 'i', 'e', 'l', 'd', 0};
  unsigned long len = strlen(name);
  TlmChar field(name);

  // Mock doing stuff in the mem around the null terminated string
  name[9] = 0xAB;

  // We should be able to use the string view to initialize
  // Name().data(), this shouldn't cause an error.
  TlmStringView field2(field.Name());

  EXPECT_EQ(field.Name().data(), name);
  EXPECT_EQ(field.Name().size(), len);

  EXPECT_EQ(field2.Name().data(), name);
  EXPECT_EQ(field2.Name().size(), len);
}
