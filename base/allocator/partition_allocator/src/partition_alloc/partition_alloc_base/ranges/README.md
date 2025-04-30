# `partition_alloc::internal::base::ranges`

This directory aims to implement a C++14 version of the new `std::ranges`
algorithms that were introduced in C++20.
These implementations has been already removed from chromium, because
chromium has already allowed C++20.
However PartitionAllocator still needs the implementations because it has not
allowed yet.
Once PartitionAllocator allows C++20, the implementations will be removed.
