// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chromium::import! {
    "//base:sequenced";
}

use rust_gtest_interop::prelude::*;
use sequenced::for_testing::{Conflict, ThreadLock, ThreadLockGuard};
use static_assertions::{assert_impl_all, assert_not_impl_any};
use std::cell::UnsafeCell;

assert_impl_all!(ThreadLock: Send, Sync);
assert_not_impl_any!(ThreadLockGuard<'static>: Send, Sync);

/// Runs `f` on a new thread, and returns its result once the thread has exited.
fn on_another_thread<R: Send>(f: impl FnOnce() -> R + Send) -> R {
    std::thread::scope(|s| s.spawn(f).join().unwrap())
}

/// Claims on a single thread follow `RefCell`'s rules.
#[gtest(RustThreadLock, ClaimsOnOneThread)]
fn test_claims_on_one_thread() {
    let lock = ThreadLock::new();

    // Shared claims nest, and block exclusive claims until the last one ends.
    let first = lock.try_claim_shared().unwrap();
    let second = lock.try_claim_shared().unwrap();
    expect_eq!(lock.try_claim_exclusive().err(), Some(Conflict::OnThisThread));
    drop(first);
    expect_eq!(lock.try_claim_exclusive().err(), Some(Conflict::OnThisThread));
    drop(second);

    // An exclusive claim blocks every other claim until it ends.
    let exclusive = lock.try_claim_exclusive().unwrap();
    expect_eq!(lock.try_claim_shared().err(), Some(Conflict::OnThisThread));
    expect_eq!(lock.try_claim_exclusive().err(), Some(Conflict::OnThisThread));
    drop(exclusive);
    expect_true!(lock.try_claim_shared().is_ok());

    // A leaked shared claim never ends, so it blocks exclusive claims for good,
    // but not shared ones.
    std::mem::forget(lock.try_claim_shared().unwrap());
    expect_true!(lock.try_claim_shared().is_ok());
    expect_eq!(lock.try_claim_exclusive().err(), Some(Conflict::OnThisThread));
}

/// Only one thread at a time can hold the lock.
#[gtest(RustThreadLock, ClaimsOnSeveralThreads)]
fn test_claims_on_several_threads() {
    let lock = ThreadLock::new();

    // While this thread holds the lock, other threads can't claim it.
    let guard = lock.try_claim_shared().unwrap();
    expect_eq!(
        on_another_thread(|| lock.try_claim_shared().err()),
        Some(Conflict::OnAnotherThread)
    );
    expect_eq!(
        on_another_thread(|| lock.try_claim_exclusive().err()),
        Some(Conflict::OnAnotherThread)
    );
    drop(guard);
    expect_true!(on_another_thread(|| lock.try_claim_exclusive().is_ok()));

    // While another thread holds the lock, this thread can't claim it.
    let claimed = std::sync::Barrier::new(2);
    let checked = std::sync::Barrier::new(2);
    std::thread::scope(|s| {
        s.spawn(|| {
            let _guard = lock.try_claim_shared().unwrap();
            claimed.wait();
            checked.wait();
        });
        // Wait until we know that the other thread has taken the lock.
        claimed.wait();
        expect_eq!(lock.try_claim_shared().err(), Some(Conflict::OnAnotherThread));
        expect_eq!(lock.try_claim_exclusive().err(), Some(Conflict::OnAnotherThread));
        checked.wait();
    });
    // `scope` has joined the other thread, so its claim has ended.
    expect_true!(lock.try_claim_exclusive().is_ok());

    // A claim leaked by a thread that has since exited blocks every thread,
    // including ones started afterwards.
    on_another_thread(|| std::mem::forget(lock.try_claim_shared().unwrap()));
    expect_eq!(
        on_another_thread(|| lock.try_claim_shared().err()),
        Some(Conflict::OnAnotherThread)
    );
    expect_eq!(lock.try_claim_shared().err(), Some(Conflict::OnAnotherThread));
}

/// Many threads contend on one `ThreadLock`, and access a value under their
/// claims. In TSan builds, a missing happens-before edge between one holder and
/// the next shows up as a data race on the value.
#[gtest(RustThreadLock, ClaimsSynchronizeAcrossThreads)]
fn test_claims_synchronize_across_threads() {
    struct Shared {
        lock: ThreadLock,
        value: UnsafeCell<u64>,
    }
    // SAFETY: We only used `Shared` in this function, and we ensure that
    // `value` is only written under an exclusive claim on `lock`, and
    // only read under a shared one.
    unsafe impl Sync for Shared {}

    /// Claims `shared.lock` over and over, and returns how many times it
    /// incremented `shared.value`.
    fn contend(shared: &Shared) -> u64 {
        let mut writes = 0;
        for i in 0..10_000 {
            if i % 2 == 0 {
                if let Ok(_guard) = shared.lock.try_claim_exclusive() {
                    // SAFETY: The exclusive claim means that no other thread is
                    // accessing `value`.
                    unsafe { *shared.value.get() += 1 };
                    writes += 1;
                }
            } else if let Ok(_outer) = shared.lock.try_claim_shared() {
                // A nested shared claim only updates the count.
                let _inner = shared.lock.try_claim_shared().unwrap();
                // `black_box` tells the compiler not to optimize this away.
                // SAFETY: The shared claim means that no thread is writing
                // `value`.
                std::hint::black_box(unsafe { *shared.value.get() });
            }
        }
        writes
    }

    let shared = Shared { lock: ThreadLock::new(), value: UnsafeCell::new(0) };
    let writes: u64 = std::thread::scope(|s| {
        // Spawn 8 threads that run 10,000 claim attempts each, and sum their
        // results.
        let threads: Vec<_> = (0..8).map(|_| s.spawn(|| contend(&shared))).collect();
        threads.into_iter().map(|thread| thread.join().unwrap()).sum()
    });
    expect_eq!(shared.value.into_inner(), writes);
}
