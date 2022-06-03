// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/win/dispatch_stub.h"
#include "base/win/scoped_bstr.h"
#include "base/win/variant_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::internal::VariantUtil;
using base::win::test::DispatchStub;

namespace base {
namespace win {

namespace {

static constexpr VARTYPE kSupportedVartypes[] = {
    VT_BOOL, VT_I1, VT_UI1, VT_I2,   VT_UI2,  VT_I4,      VT_UI4,     VT_I8,
    VT_UI8,  VT_R4, VT_R8,  VT_DATE, VT_BSTR, VT_UNKNOWN, VT_DISPATCH};

template <VARTYPE ElementVartype>
static bool TestIsConvertibleTo(const std::set<VARTYPE>& allowed_vartypes) {
  for (VARTYPE vartype : kSupportedVartypes) {
    if (VariantUtil<ElementVartype>::IsConvertibleTo(vartype) !=
        base::Contains(allowed_vartypes, vartype)) {
      return false;
    }
  }
  return true;
}

template <VARTYPE ElementVartype>
static bool TestIsConvertibleFrom(const std::set<VARTYPE>& allowed_vartypes) {
  for (VARTYPE vartype : kSupportedVartypes) {
    if (VariantUtil<ElementVartype>::IsConvertibleFrom(vartype) !=
        base::Contains(allowed_vartypes, vartype)) {
      return false;
    }
  }
  return true;
}

}  // namespace

TEST(VariantUtilTest, VariantTypeBool) {
  VARIANT variant;
  V_VT(&variant) = VT_BOOL;

  VariantUtil<VT_BOOL>::RawSet(&variant, VARIANT_TRUE);
  EXPECT_EQ(V_BOOL(&variant), VARIANT_TRUE);
  EXPECT_EQ(VariantUtil<VT_BOOL>::RawGet(variant), VARIANT_TRUE);

  const std::set<VARTYPE> allowed_vartypes = {VT_BOOL};
  EXPECT_TRUE(TestIsConvertibleTo<VT_BOOL>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_BOOL>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeI1) {
  VARIANT variant;
  V_VT(&variant) = VT_I1;

  VariantUtil<VT_I1>::RawSet(&variant, 34);
  EXPECT_EQ(V_I1(&variant), 34);
  EXPECT_EQ(VariantUtil<VT_I1>::RawGet(variant), 34);

  const std::set<VARTYPE> allowed_vartypes = {VT_I1};
  EXPECT_TRUE(TestIsConvertibleTo<VT_I1>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_I1>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeUI1) {
  VARIANT variant;
  V_VT(&variant) = VT_UI1;

  VariantUtil<VT_UI1>::RawSet(&variant, 34U);
  EXPECT_EQ(V_UI1(&variant), 34U);
  EXPECT_EQ(VariantUtil<VT_UI1>::RawGet(variant), 34U);

  const std::set<VARTYPE> allowed_vartypes = {VT_UI1};
  EXPECT_TRUE(TestIsConvertibleTo<VT_UI1>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_UI1>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeI2) {
  VARIANT variant;
  V_VT(&variant) = VT_I2;

  VariantUtil<VT_I2>::RawSet(&variant, 8738);
  EXPECT_EQ(V_I2(&variant), 8738);
  EXPECT_EQ(VariantUtil<VT_I2>::RawGet(variant), 8738);

  const std::set<VARTYPE> allowed_vartypes = {VT_I2};
  EXPECT_TRUE(TestIsConvertibleTo<VT_I2>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_I2>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeUI2) {
  VARIANT variant;
  V_VT(&variant) = VT_UI2;

  VariantUtil<VT_UI2>::RawSet(&variant, 8738U);
  EXPECT_EQ(V_UI2(&variant), 8738U);
  EXPECT_EQ(VariantUtil<VT_UI2>::RawGet(variant), 8738U);

  const std::set<VARTYPE> allowed_vartypes = {VT_UI2};
  EXPECT_TRUE(TestIsConvertibleTo<VT_UI2>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_UI2>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeI4) {
  VARIANT variant;
  V_VT(&variant) = VT_I4;

  VariantUtil<VT_I4>::RawSet(&variant, 572662306);
  EXPECT_EQ(V_I4(&variant), 572662306);
  EXPECT_EQ(VariantUtil<VT_I4>::RawGet(variant), 572662306);

  const std::set<VARTYPE> allowed_vartypes = {VT_I4};
  EXPECT_TRUE(TestIsConvertibleTo<VT_I4>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_I4>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeUI4) {
  VARIANT variant;
  V_VT(&variant) = VT_UI4;

  VariantUtil<VT_UI4>::RawSet(&variant, 572662306U);
  EXPECT_EQ(V_UI4(&variant), 572662306U);
  EXPECT_EQ(VariantUtil<VT_UI4>::RawGet(variant), 572662306U);

  const std::set<VARTYPE> allowed_vartypes = {VT_UI4};
  EXPECT_TRUE(TestIsConvertibleTo<VT_UI4>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_UI4>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeI8) {
  VARIANT variant;
  V_VT(&variant) = VT_I8;

  VariantUtil<VT_I8>::RawSet(&variant, 2459565876494606882);
  EXPECT_EQ(V_I8(&variant), 2459565876494606882);
  EXPECT_EQ(VariantUtil<VT_I8>::RawGet(variant), 2459565876494606882);

  const std::set<VARTYPE> allowed_vartypes = {VT_I8};
  EXPECT_TRUE(TestIsConvertibleTo<VT_I8>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_I8>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeUI8) {
  VARIANT variant;
  V_VT(&variant) = VT_UI8;

  VariantUtil<VT_UI8>::RawSet(&variant, 2459565876494606882U);
  EXPECT_EQ(V_UI8(&variant), 2459565876494606882U);
  EXPECT_EQ(VariantUtil<VT_UI8>::RawGet(variant), 2459565876494606882U);

  const std::set<VARTYPE> allowed_vartypes = {VT_UI8};
  EXPECT_TRUE(TestIsConvertibleTo<VT_UI8>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_UI8>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeR4) {
  VARIANT variant;
  V_VT(&variant) = VT_R4;

  VariantUtil<VT_R4>::RawSet(&variant, 3.14159f);
  EXPECT_EQ(V_R4(&variant), 3.14159f);
  EXPECT_EQ(VariantUtil<VT_R4>::RawGet(variant), 3.14159f);

  const std::set<VARTYPE> allowed_vartypes = {VT_R4};
  EXPECT_TRUE(TestIsConvertibleTo<VT_R4>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_R4>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeR8) {
  VARIANT variant;
  V_VT(&variant) = VT_R8;

  VariantUtil<VT_R8>::RawSet(&variant, 3.14159);
  EXPECT_EQ(V_R8(&variant), 3.14159);
  EXPECT_EQ(VariantUtil<VT_R8>::RawGet(variant), 3.14159);

  const std::set<VARTYPE> allowed_vartypes = {VT_R8};
  EXPECT_TRUE(TestIsConvertibleTo<VT_R8>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_R8>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeDate) {
  SYSTEMTIME sys_time;
  ::GetSystemTime(&sys_time);
  DATE date;
  ::SystemTimeToVariantTime(&sys_time, &date);

  VARIANT variant;
  V_VT(&variant) = VT_DATE;

  VariantUtil<VT_DATE>::RawSet(&variant, date);
  EXPECT_EQ(V_DATE(&variant), date);
  EXPECT_EQ(VariantUtil<VT_DATE>::RawGet(variant), date);

  const std::set<VARTYPE> allowed_vartypes = {VT_DATE};
  EXPECT_TRUE(TestIsConvertibleTo<VT_DATE>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_DATE>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeBstr) {
  ScopedBstr scoped_bstr;
  scoped_bstr.Allocate(L"some text");

  VARIANT variant;
  V_VT(&variant) = VT_BSTR;

  VariantUtil<VT_BSTR>::RawSet(&variant, scoped_bstr.Get());
  EXPECT_EQ(V_BSTR(&variant), scoped_bstr.Get());
  EXPECT_EQ(VariantUtil<VT_BSTR>::RawGet(variant), scoped_bstr.Get());

  const std::set<VARTYPE> allowed_vartypes = {VT_BSTR};
  EXPECT_TRUE(TestIsConvertibleTo<VT_BSTR>(allowed_vartypes));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_BSTR>(allowed_vartypes));
}

TEST(VariantUtilTest, VariantTypeUnknown) {
  Microsoft::WRL::ComPtr<IUnknown> unknown =
      Microsoft::WRL::Make<DispatchStub>();

  VARIANT variant;
  V_VT(&variant) = VT_UNKNOWN;

  VariantUtil<VT_UNKNOWN>::RawSet(&variant, unknown.Get());
  EXPECT_EQ(V_UNKNOWN(&variant), unknown.Get());
  EXPECT_EQ(VariantUtil<VT_UNKNOWN>::RawGet(variant), unknown.Get());

  const std::set<VARTYPE> allow_convertible_to = {VT_UNKNOWN};
  const std::set<VARTYPE> allow_convertible_from = {VT_UNKNOWN, VT_DISPATCH};
  EXPECT_TRUE(TestIsConvertibleTo<VT_UNKNOWN>(allow_convertible_to));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_UNKNOWN>(allow_convertible_from));
}

TEST(VariantUtilTest, VariantTypeDispatch) {
  Microsoft::WRL::ComPtr<IDispatch> dispatch =
      Microsoft::WRL::Make<DispatchStub>();

  VARIANT variant;
  V_VT(&variant) = VT_DISPATCH;

  VariantUtil<VT_DISPATCH>::RawSet(&variant, dispatch.Get());
  EXPECT_EQ(V_DISPATCH(&variant), dispatch.Get());
  EXPECT_EQ(VariantUtil<VT_DISPATCH>::RawGet(variant), dispatch.Get());

  const std::set<VARTYPE> allow_convertible_to = {VT_UNKNOWN, VT_DISPATCH};
  const std::set<VARTYPE> allow_convertible_from = {VT_DISPATCH};
  EXPECT_TRUE(TestIsConvertibleTo<VT_DISPATCH>(allow_convertible_to));
  EXPECT_TRUE(TestIsConvertibleFrom<VT_DISPATCH>(allow_convertible_from));
}

}  // namespace win
}  // namespace base
