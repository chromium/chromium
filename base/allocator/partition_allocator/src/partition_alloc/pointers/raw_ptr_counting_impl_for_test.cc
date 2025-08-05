// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/pointers/raw_ptr_counting_impl_for_test.h"

int base::test::RawPtrCountingImplForTest::wrap_raw_ptr_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::release_wrapped_ptr_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::get_for_dereference_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::get_for_extraction_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::get_for_comparison_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::wrapped_ptr_swap_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::wrapped_ptr_less_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::pointer_to_member_operator_cnt =
    INT_MIN;
int base::test::RawPtrCountingImplForTest::wrap_raw_ptr_for_dup_cnt = INT_MIN;
int base::test::RawPtrCountingImplForTest::get_for_duplication_cnt = INT_MIN;
