// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This crate defines the `ScopedRefPtr` type, which is a Rust wrapper for a
//! `base::scoped_refptr`. Therefore, it only accepts types which match the
//! ref-counting behavior of `base::scoped_refptr`, and only works on opaque
//! C++ types.
//!
//! `ScopedRefPtr` relies heavily on the fact that opaque C++ values are
//! represented in Rust using zero-sized types (ZSTs), and it is therefore
//! permissible to have multiple mutable references, and to mutate the value
//! while a shared reference is held. This is because ZSTs take up no memory
//! (as far as Rust knows), and so their references cannot overlap or alias,
//! and thus it is impossible for them to violate Rust's aliasing rules.
//!
//! Of course, when used to represent an opaque C++ object, the pointed-to
//! object does take up space (Rust just doesn't know or care), so cxx exposes
//! mutable references behind a Pin. This is to prevent surprising behavior: for
//! example, `mem::swap`ing two opaque cxx objects is a no-op (because Rust has
//! no information about their memory layout).
//!
//! Since it's possible to have multiple mutable references to a ZST, care must
//! be taken when invoking functions that are not thread-safe. Such functions
//! should always be `unsafe` in Rust.

use std::ptr::NonNull;

/// Indicates an opaque C++ type with an internal mechanism for ref-counting.
///
/// # Safety
/// The `impl` must guarantee that `T` can only be destroyed
/// when the last ref-count is given up by calling `Release`.
/// (For example, it must not be possible to allocate `T` on the stack.)
pub unsafe trait CxxRefCounted: cxx::ExternType<Kind = cxx::kind::Opaque> + 'static {
    /// Increments the object's ref-count. Safe to call, but can cause memory
    /// leaks if the object isn't appropriately `release`d later.
    ///
    /// Note that it's not legal to call this if the ref count is 0, but so
    /// long as the object is coming from C++, the ref count of the rust
    /// representation will always be at least 1.
    ///
    /// This type takes &self for compatibility with the C++ signature, and
    /// because increasing the ref-count doesn't logically mutate the object.
    fn add_ref(&self);

    /// Decrement the object's ref count. This may lead to the object being
    /// deallocated, so this function demands an exclusive reference. Logically,
    /// it should take `self` by value, but we want to call it from `drop`,
    /// which only provides a mutable reference.
    ///
    /// # Safety
    /// The caller must ensure that:
    /// 1. They own 1 of this object's ref-counts.
    /// 2. No code will dereference `self` after the call.
    unsafe fn release(&self);
}

/// # Safety
///
/// An impl of this trait indicating that the implementation of CxxRefCounted
/// for `T` is thread-safe, and therefore it is safe to use `ScopedRefPtr<T>`
/// concurrently, provided it's safe to use `T` alone.
///
/// Note that thread-safety for `T` is more subtle for C++ types than in Rust.
/// `Send` and `Sync` should only be implemented for a type `T` after careful
/// inspection of its implementation. If a type is mostly thread-safe but has
/// some non-thread-safe methods, you may want to implement `Send` and `Sync`
/// and ensure that the non-thread-safe methods are marked `unsafe`.
pub unsafe trait CxxRefCountedThreadSafe: CxxRefCounted {}

/// A pointer to an object which manages its own ref count. The ref count impl
/// guarantees that the object will not be dropped while the pointer remains.
///
/// Safety implications:
///
/// 1. The contained pointer is always non-null and can be safely dereferenced
///    as long as the ScopedRefPtr is alive.
/// 2. This type DOES NOT guarantee exclusive access to the pointed-to object,
///    even if you have a mutable reference! The user is responsible for
///    ensuring any potential concurrent accesses are safe.
pub struct ScopedRefPtr<T: CxxRefCounted> {
    /// # Safety invariants
    ///
    /// * `ptr` is non-dangling, correctly aligned, and points to valid data
    ///   (based on ref-counting guarantees of `unsafe fn wrap_ref_counted` and
    ///   `impl<T> Drop for ScopedRefPtr<T>`)
    /// * `T` is a ZST (a zero-sized type) so Rust aliasing/exclusivity
    ///   requirements do not apply to `&mut T` (based on the `CxxRefCounted:
    ///   ExternType<Kind = cxx::kind::Opaque>` constraint).
    ptr: NonNull<T>,
}

impl<T: CxxRefCounted> ScopedRefPtr<T> {
    /// Create a new ScopedRefPtr from a pointer to a C++ value which was
    /// obtained from a C++ scoped_refptr.
    ///
    /// # Safety
    ///
    /// `ptr` must come from a C++ `scoped_refptr` to `T` which gave up
    /// ownership without decrementing the ref count (e.g. by calling
    /// `release()`).
    pub unsafe fn wrap_ref_counted(ptr: *mut T) -> Option<ScopedRefPtr<T>> {
        NonNull::new(ptr).map(|nonnull| ScopedRefPtr { ptr: nonnull })
    }

    /// Returns a pinned mutable reference to the stored object. Note that
    /// because T is opaque, this reference does _not_ ensure the object is not
    /// mutated while the reference exists, and multiple mutable references can
    /// co-exist.
    ///
    /// Note that this function takes &self, not &mut self, because it is valid
    /// for multiple mutable references to a zero-sized type to exist.
    ///
    /// Note as well that the pin here isn't really doing anything, because T
    /// is zero-sized. However, attempting to move out of the reference won't do
    /// anything, for the same reason, so the pin exists to prevent surprising
    /// behavior. The return value of this function should typically only be
    /// used as an argument to C++ FFI methods.
    #[allow(clippy::mut_from_ref)]
    #[allow(mutable_transmutes)]
    pub fn as_pin(&self) -> std::pin::Pin<&mut T> {
        let cpp_obj_ref: &T = self; // Via `impl Deref`.

        // SAFETY: `&mut` exclusivity/aliasing rules don't apply to ZSTs
        // (based on the `cxx::kind::Opaque` constraint of the type).
        assert!(std::mem::size_of::<T>() == 0);
        let cpp_obj_mut_ref = unsafe { std::mem::transmute::<&T, &mut T>(cpp_obj_ref) };

        // SAFETY:
        // * No public APIs expose `&mut T`
        // * Private APIs don't move the underlying data
        unsafe { std::pin::Pin::new_unchecked(cpp_obj_mut_ref) }
    }
}

impl<T: CxxRefCounted> Drop for ScopedRefPtr<T> {
    /// Decrement the object's ref-count before it goes out of scope.
    fn drop(&mut self) {
        // SAFETY: We own one ref-count of this object (either from wrapping
        // a released pointer, or cloning an existing one), and we're dropping
        // it so we know it won't be referenced any more.
        unsafe { self.release() };
    }
}

impl<T: CxxRefCounted> Clone for ScopedRefPtr<T> {
    /// Clone the pointer, incrementing the value's ref-count.
    fn clone(&self) -> Self {
        self.add_ref();
        ScopedRefPtr { ptr: self.ptr }
    }
}

// Note that because T is opaque, this reference does _not_ ensure the object is
// not mutated while the reference exists.
impl<T: CxxRefCounted> std::ops::Deref for ScopedRefPtr<T> {
    type Target = T;

    fn deref(&self) -> &T {
        // SAFETY: The safety invariants of the `ptr` field guarantee that
        // aliasing rules don't apply (`T` is a ZST) and that `ptr` points to
        // valid, aligned data.
        assert!(std::mem::size_of::<T>() == 0);
        unsafe { self.ptr.as_ref() }
    }
}

// New scope so the `use std::fmt` doesn't pollute the rest of the file
const _: () = {
    use std::fmt::{Debug, Display, Error, Formatter};

    impl<T: CxxRefCounted + Debug> Debug for ScopedRefPtr<T> {
        fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
            Debug::fmt(&**self, f)
        }
    }

    impl<T: CxxRefCounted + Display> Display for ScopedRefPtr<T> {
        fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
            Display::fmt(&**self, f)
        }
    }
};

// SAFETY: it's safe to send/share T, and the CxxRefCountedThreadSafe
// implementation guarantees that the ref-counting is itself thread-safe.
unsafe impl<T: Send + Sync + CxxRefCountedThreadSafe> Send for ScopedRefPtr<T> {}
unsafe impl<T: Send + Sync + CxxRefCountedThreadSafe> Sync for ScopedRefPtr<T> {}
