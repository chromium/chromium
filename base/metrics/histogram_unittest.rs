// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:histogram";
    "//base/test:histogram_tester";
}

use histogram::UmaEnum;
use histogram_tester::HistogramTester;
use rust_gtest_interop::prelude::*;

#[allow(dead_code)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, UmaEnum)]
#[repr(i32)]
enum TestEnum {
    Zero = 0,
    One = 1,
    Two = 2,
}

#[allow(dead_code)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, UmaEnum)]
#[repr(u32)]
enum U32Enum {
    First = 0,
    Second = 10,
}

#[allow(dead_code)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, UmaEnum)]
#[repr(u8)]
enum U8Enum {
    Low = 0,
    High = 250,
}

#[gtest(RustHistogramTest, UmaHistogramBoolean)]
fn test_uma_histogram_boolean() {
    let tester = HistogramTester::new();
    histogram::record_bool("Test.Rust.Boolean", true);
    histogram::record_bool("Test.Rust.Boolean", false);
    histogram::record_bool("Test.Rust.Boolean", true);

    tester.expect_bucket_count("Test.Rust.Boolean", 1, 2); // true
    tester.expect_bucket_count("Test.Rust.Boolean", 0, 1); // false
    tester.expect_total_count("Test.Rust.Boolean", 3);
}

#[gtest(RustHistogramTest, UmaHistogramExactLinear)]
fn test_uma_histogram_exact_linear() {
    let tester = HistogramTester::new();
    histogram::record_exact_linear("Test.Rust.ExactLinear", 5, 10);
    histogram::record_exact_linear("Test.Rust.ExactLinear", 5, 10);
    histogram::record_exact_linear("Test.Rust.ExactLinear", 11, 10); // goes to overflow bucket

    tester.expect_bucket_count("Test.Rust.ExactLinear", 5, 2);
    tester.expect_bucket_count("Test.Rust.ExactLinear", 10, 1); // 10 is the exclusive_max (overflow bucket)
    tester.expect_total_count("Test.Rust.ExactLinear", 3);
}

#[gtest(RustHistogramTest, UmaHistogramEnumeration)]
fn test_uma_histogram_enumeration() {
    assert_eq!(TestEnum::MAX_VALUE, 2);
    assert_eq!(U32Enum::MAX_VALUE, 10);
    assert_eq!(U8Enum::MAX_VALUE, 250);

    let tester = HistogramTester::new();
    histogram::record_enum("Test.Rust.Enumeration", TestEnum::Zero);
    histogram::record_enum("Test.Rust.Enumeration", TestEnum::One);
    histogram::record_enum("Test.Rust.Enumeration", TestEnum::Two);
    histogram::record_enum("Test.Rust.Enumeration", TestEnum::Two);

    tester.expect_bucket_count("Test.Rust.Enumeration", 0, 1);
    tester.expect_bucket_count("Test.Rust.Enumeration", 1, 1);
    tester.expect_bucket_count("Test.Rust.Enumeration", 2, 2);
    tester.expect_total_count("Test.Rust.Enumeration", 4);

    histogram::record_enum("Test.Rust.U32Enumeration", U32Enum::Second);
    tester.expect_bucket_count("Test.Rust.U32Enumeration", 10, 1);

    histogram::record_enum("Test.Rust.U8Enumeration", U8Enum::High);
    tester.expect_bucket_count("Test.Rust.U8Enumeration", 250, 1);
}

#[gtest(RustHistogramTest, UmaHistogramPercentage)]
fn test_uma_histogram_percentage() {
    let tester = HistogramTester::new();
    histogram::record_percentage("Test.Rust.Percentage", 85);
    histogram::record_percentage("Test.Rust.Percentage", 85);

    tester.expect_bucket_count("Test.Rust.Percentage", 85, 2);
    tester.expect_total_count("Test.Rust.Percentage", 2);
}

#[gtest(RustHistogramTest, UmaHistogramSparse)]
fn test_uma_histogram_sparse() {
    let tester = HistogramTester::new();
    histogram::record_sparse("Test.Rust.Sparse", 123456);
    histogram::record_sparse("Test.Rust.Sparse", 123456);

    tester.expect_bucket_count("Test.Rust.Sparse", 123456, 2);
    tester.expect_total_count("Test.Rust.Sparse", 2);
}

#[gtest(RustHistogramTest, UmaHistogramCustomCounts)]
fn test_uma_histogram_custom_counts() {
    let tester = HistogramTester::new();
    histogram::record_custom_counts("Test.Rust.CustomCounts", 0, 1, 100, 10);
    histogram::record_custom_counts("Test.Rust.CustomCounts", 20, 1, 100, 10);
    histogram::record_custom_counts("Test.Rust.CustomCounts", 20, 1, 100, 10);
    histogram::record_custom_counts("Test.Rust.CustomCounts", 50, 1, 100, 10);
    histogram::record_custom_counts("Test.Rust.CustomCounts", 105, 1, 100, 10);

    tester.expect_bucket_count("Test.Rust.CustomCounts", 0, 1);
    tester.expect_bucket_count("Test.Rust.CustomCounts", 20, 2);
    tester.expect_bucket_count("Test.Rust.CustomCounts", 50, 1);
    tester.expect_bucket_count("Test.Rust.CustomCounts", 100, 1);
    tester.expect_total_count("Test.Rust.CustomCounts", 5);
}

#[gtest(RustHistogramTest, UmaHistogramCounts)]
fn test_uma_histogram_counts() {
    let tester = HistogramTester::new();
    histogram::record_counts_100("Test.Rust.Counts100", 42);
    tester.expect_unique_sample("Test.Rust.Counts100", 42, 1);

    histogram::record_counts_1000("Test.Rust.Counts1000", 420);
    tester.expect_unique_sample("Test.Rust.Counts1000", 420, 1);

    histogram::record_counts_10000("Test.Rust.Counts10000", 4200);
    tester.expect_unique_sample("Test.Rust.Counts10000", 4200, 1);

    histogram::record_counts_100000("Test.Rust.Counts100000", 42000);
    tester.expect_unique_sample("Test.Rust.Counts100000", 42000, 1);

    histogram::record_counts_1m("Test.Rust.Counts1M", 420000);
    tester.expect_unique_sample("Test.Rust.Counts1M", 420000, 1);

    histogram::record_counts_10m("Test.Rust.Counts10M", 4200000);
    tester.expect_unique_sample("Test.Rust.Counts10M", 4200000, 1);
}
