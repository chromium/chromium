// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_policy_fetcher_win_util.h"

#include <OleCtl.h>
#include <wtypes.h>

#include "base/test/bind.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct MockPolicyStatusValue : public IPolicyStatusValue {
 public:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, get_source, HRESULT(BSTR*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE, get_value, HRESULT(BSTR*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_hasConflict,
                             HRESULT(VARIANT_BOOL*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_conflictSource,
                             HRESULT(BSTR*));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_conflictValue,
                             HRESULT(BSTR*));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT*));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo**));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR*, UINT, LCID, DISPID*));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID,
                                     REFIID,
                                     LCID,
                                     WORD,
                                     DISPPARAMS*,
                                     VARIANT*,
                                     EXCEPINFO*,
                                     UINT*));

  // IUnknown:
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             QueryInterface,
                             HRESULT(REFIID,
                                     _COM_Outptr_ void __RPC_FAR* __RPC_FAR*));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, AddRef, ULONG(void));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE, Release, ULONG(void));
};

}  // namespace

TEST(ConvertPolicyStatusValueToPolicyEntry, DefaultSource) {
  MockPolicyStatusValue policy_status_value;
  base::win::ScopedBstr value(L"value");
  base::win::ScopedBstr source(L"Default");
  VARIANT_BOOL has_conflict = VARIANT_FALSE;
  EXPECT_CALL(policy_status_value, get_value(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(value.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_source(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(source.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_hasConflict(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(has_conflict),
                               testing::Return(S_OK)));

  policy::PolicyMap::Entry expected(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value("value"), nullptr);
  expected.SetIsDefaultValue();

  auto actual = ConvertPolicyStatusValueToPolicyEntry(
      &policy_status_value, PolicyValueOverrideFunction());
  EXPECT_TRUE(expected.Equals(*actual));
}

TEST(ConvertPolicyStatusValueToPolicyEntry, CloudSource) {
  MockPolicyStatusValue policy_status_value;
  base::win::ScopedBstr value(L"1");
  base::win::ScopedBstr source(L"Device Management");
  VARIANT_BOOL has_conflict = VARIANT_FALSE;
  EXPECT_CALL(policy_status_value, get_value(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(value.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_source(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(source.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_hasConflict(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(has_conflict),
                               testing::Return(S_OK)));

  policy::PolicyMap::Entry expected(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_CLOUD, base::Value("1"), nullptr);

  auto actual = ConvertPolicyStatusValueToPolicyEntry(
      &policy_status_value, PolicyValueOverrideFunction());
  EXPECT_TRUE(expected.Equals(*actual));
}

TEST(ConvertPolicyStatusValueToPolicyEntry, Conflict) {
  MockPolicyStatusValue policy_status_value;
  base::win::ScopedBstr value(L"a");
  base::win::ScopedBstr source(L"Group Policy");
  VARIANT_BOOL has_conflict = VARIANT_TRUE;
  base::win::ScopedBstr conflict_value(L"ab");
  base::win::ScopedBstr conflict_source(L"Device Management");
  EXPECT_CALL(policy_status_value, get_value(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(value.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_source(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(source.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_hasConflict(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(has_conflict),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_conflictValue(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(conflict_value.Get()),
                               testing::Return(S_OK)));
  EXPECT_CALL(policy_status_value, get_conflictSource(testing::_))
      .WillOnce(testing::DoAll(testing::SetArgPointee<0>(conflict_source.Get()),
                               testing::Return(S_OK)));

  policy::PolicyMap::Entry expected(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_PLATFORM, base::Value(false), nullptr);
  policy::PolicyMap::Entry conflict(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  expected.AddConflictingPolicy(std::move(conflict));
  const auto override_value = [](BSTR initial_value) {
    return base::Value(::SysStringLen(initial_value) > 1);
  };

  auto actual = ConvertPolicyStatusValueToPolicyEntry(
      &policy_status_value, base::BindRepeating(override_value));
  EXPECT_TRUE(expected.Equals(*actual));
}

TEST(ConvertPolicyStatusValueToPolicyEntry, ValueError) {
  MockPolicyStatusValue policy_status_value;
  base::win::ScopedBstr value(L"a");
  base::win::ScopedBstr source(L"Group Policy");
  VARIANT_BOOL has_conflict = VARIANT_TRUE;
  base::win::ScopedBstr conflict_value(L"ab");
  base::win::ScopedBstr conflict_source(L"Device Management");

  policy::PolicyMap::Entry expected(
      policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
      policy::POLICY_SOURCE_PLATFORM, base::Value("a"), nullptr);

  // No policy created if we fail to get the value.
  EXPECT_CALL(policy_status_value, get_value(testing::_))
      .WillOnce(testing::Return(-1))
      .WillRepeatedly(testing::DoAll(testing::SetArgPointee<0>(value.Get()),
                                     testing::Return(S_OK)));
  EXPECT_EQ(nullptr, ConvertPolicyStatusValueToPolicyEntry(
                         &policy_status_value, PolicyValueOverrideFunction())
                         .get());

  // No policy created if we fail to get the source.
  EXPECT_CALL(policy_status_value, get_source(testing::_))
      .WillOnce(testing::Return(-1))
      .WillRepeatedly(testing::DoAll(testing::SetArgPointee<0>(source.Get()),
                                     testing::Return(S_OK)));
  EXPECT_EQ(nullptr, ConvertPolicyStatusValueToPolicyEntry(
                         &policy_status_value, PolicyValueOverrideFunction())
                         .get());

  // No conflict added if we fail to get any of the conflict info.
  EXPECT_CALL(policy_status_value, get_hasConflict(testing::_))
      .WillOnce(testing::Return(-1))
      .WillRepeatedly(testing::DoAll(testing::SetArgPointee<0>(has_conflict),
                                     testing::Return(S_OK)));
  EXPECT_TRUE(expected.Equals(*ConvertPolicyStatusValueToPolicyEntry(
      &policy_status_value, PolicyValueOverrideFunction())));

  // No conflict added if we fail to get any of the conflict value.
  EXPECT_CALL(policy_status_value, get_conflictValue(testing::_))
      .WillOnce(testing::Return(-1))
      .WillRepeatedly(
          testing::DoAll(testing::SetArgPointee<0>(conflict_value.Get()),
                         testing::Return(S_OK)));
  EXPECT_TRUE(expected.Equals(*ConvertPolicyStatusValueToPolicyEntry(
      &policy_status_value, PolicyValueOverrideFunction())));

  // No conflict added if we fail to get any of the conflict info.
  EXPECT_CALL(policy_status_value, get_conflictSource(testing::_))
      .WillOnce(testing::Return(-1));
  EXPECT_TRUE(expected.Equals(*ConvertPolicyStatusValueToPolicyEntry(
      &policy_status_value, PolicyValueOverrideFunction())));
}
