// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This module defines the [`ThreadLock`] type, a non-blocking lock which
//! allows either one exclusive reference or multiple shared references to
//! the protected data, but only on a single thread at a time. Attempts to
//! acquire a reference on multiple threads simultaneously will fail.

use std::cell::Cell;
use std::marker::PhantomData;
use std::sync::atomic::AtomicU32;
use std::sync::atomic::Ordering::{Acquire, Relaxed, Release};

#[cxx::bridge(namespace = "base")]
mod ffi {
    unsafe extern "C++" {
        include!("base/task/sequenced_rust_ffi.h");

        // Returns an index for the current thread that's never 0 and never
        // reused.
        //
        // `std::thread::ThreadId` does exactly this, but in component builds it
        // gets statically linked into each component, so we wouldn't get
        // consistent IDs. Furthermore, we can't store them in an atomic because
        // `as_u64` is unstable.
        //
        // OS thread IDs have a similar problem, plus they get reused over time.
        //
        // To ensure uniqueness across chromium, there's only a single counter
        // that lives in the `base` component. Therefore every thread gets the
        // same value every time it calls this function.
        fn GetThreadIndexForRust() -> u32;
    }
}

/// A lock which ensures the protected data is only accessed on a single thread
/// at once. On that thread, it allows either mutable or shared references.
///
/// This type never blocks; any failed claim returns [`Conflict`] instead.
#[derive(Default)]
pub struct ThreadLock {
    /// Index of the thread holding the lock, or 0 if unset. Only set
    /// via a compare-and-swap (if it was 0), and only reset by the
    /// thread it names (if it was nonzero).
    owner: AtomicU32,
    /// Counts the number of existing guards. 0 indicates no guards; -1
    /// (`ThreadLock::EXCLUSIVE`) indicates a mutable guard; a number > 0
    /// indicates that many shared guards. Only the thread named in `owner`
    /// reads or writes this.
    guards: Cell<i32>,
}

/// Proof that this thread holds a claim on a `ThreadLock`. Dropping it ends the
/// claim.
pub struct ThreadLockGuard<'a> {
    lock: &'a ThreadLock,
    /// Makes `ThreadLockGuard` `!Send` and `!Sync`, since it's meant for use
    /// only on the thread where it was created.
    _not_send: PhantomData<*const ()>,
}

/// The reason a claim failed.
#[derive(Debug, PartialEq, Eq)]
pub enum Conflict {
    /// This thread already holds an incompatible reference type.
    OnThisThread,
    /// Another thread holds a reference.
    OnAnotherThread,
}

impl ThreadLock {
    /// The value of `ThreadLock::guards` while an exclusive guard exists.
    const EXCLUSIVE: i32 = -1;

    /// Creates a lock that no thread holds.
    pub const fn new() -> Self {
        Self { owner: AtomicU32::new(0), guards: Cell::new(0) }
    }

    /// Takes the lock for the current thread, if no thread holds it.
    /// Note that this will fail even if the current thread already holds
    /// the lock, so that exclusive claims can detect that they should fail.
    fn take_lock(&self) -> Result<(), Conflict> {
        let me = ffi::GetThreadIndexForRust();
        // If two threads race here, the compare-and-swap lets exactly one win.
        // `Acquire` pairs with the `Release` in `ThreadLockGuard::drop`.
        match self.owner.compare_exchange(0, me, Acquire, Relaxed) {
            Ok(_) => Ok(()),
            Err(owner) if owner == me => Err(Conflict::OnThisThread),
            Err(_) => Err(Conflict::OnAnotherThread),
        }
    }

    /// Takes an exclusive claim. Fails if any thread holds the lock.
    pub fn try_claim_exclusive(&self) -> Result<ThreadLockGuard<'_>, Conflict> {
        self.take_lock()?;
        self.guards.set(Self::EXCLUSIVE);
        Ok(ThreadLockGuard { lock: self, _not_send: PhantomData })
    }

    /// Takes a shared claim. Fails if this thread holds an exclusive claim, or
    /// if another thread holds the lock. Panics if this thread already holds
    /// `i32::MAX` shared claims.
    pub fn try_claim_shared(&self) -> Result<ThreadLockGuard<'_>, Conflict> {
        match self.take_lock() {
            Ok(()) => self.guards.set(1),
            Err(Conflict::OnThisThread) => {
                // This thread already holds the lock, so it owns `guards`.
                // Therefore, the operations are safe and not racy.
                let count = self.guards.get();
                if count == Self::EXCLUSIVE {
                    return Err(Conflict::OnThisThread);
                }
                // If there would be more than `i32::MAX` guards...just panic.
                self.guards.set(count.checked_add(1).expect("too many shared claims"));
            }
            Err(Conflict::OnAnotherThread) => return Err(Conflict::OnAnotherThread),
        }
        Ok(ThreadLockGuard { lock: self, _not_send: PhantomData })
    }
}

impl Drop for ThreadLockGuard<'_> {
    /// Release this claim on the `ThreadLock`.
    fn drop(&mut self) {
        let lock = self.lock;
        let count = lock.guards.get();
        if count == ThreadLock::EXCLUSIVE || count == 1 {
            lock.guards.set(0);
            // This is the last guard. `Release` pairs with the next claim's
            // `Acquire`, so the next holder sees every write made under this
            // claim.
            lock.owner.store(0, Release);
        } else {
            // Note that this is guaranteed to be running on the thread that
            // holds the lock, so this isn't racy.
            lock.guards.set(count - 1);
        }
    }
}

// SAFETY: `owner` is atomic, and only the thread named in `owner` reads or
// writes `guards`.
//
// The `Release` store in `ThreadLockGuard::drop` that releases the lock pairs
// with the `Acquire` compare-and-swap that takes it, so everything one holder
// does happens-before everything the next holder does.
unsafe impl Sync for ThreadLock {}
