# //base/memory Types

## Overview
This directory contains a variety of pointer-like objects (aka smart pointers).
This is a brief overview of what they are and how they should be used. Refer to
individual header files for details. C++ is not memory safe, so use these types
to help guard against potential memory bugs.
There are other pointer-like object types implemented elsewhere that may be
right for a given use case, such as `std::optional<T>` and
`std::unique_ptr<T>`. More on all types in video form
[here](https://youtu.be/MpwbWSEDfjM?t=582s) and in a doc
[here](https://docs.google.com/document/d/1VRevv8JhlP4I8fIlvf87IrW2IRjE0PbkSfIcI6-UbJo/edit?usp=sharing).

## `raw_ptr<T>`
Use for class fields/members that would otherwise be a `T*`.

This is a weakly refcounted wrapper for a `T*` (also called a raw
pointer). When the object is deleted, the allocator will "poison" the memory
that object occupied and keep the memory around so it’s not reused. This reduces
the risk and impact of a use-after-free bug.

Depending on the use case, it's possible a smart pointer with additional
features would be more appropriate, but if none of those are applicable or
necessary, `raw_ptr<T>` is preferred over a `T*`.

For more information, see [`raw_ptr.md`](./raw_ptr.md); for guidance on
usage, see
[the style guide](../../styleguide/c++/c++.md#non_owning-pointers-in-class-fields).

## `raw_ref<T>`
Use for class fields/members that would otherwise be a `T&`.

This shares much in common with `raw_ptr<T>`, but asserts that the
`raw_ref<T>` is not nullable.

For more information, see [`raw_ptr.md`](./raw_ptr.md); for guidance on
usage, see
[the style guide](../../styleguide/c++/c++.md#non_owning-pointers-in-class-fields).

## `base::WeakPtr<T>`
Use when a reference to an object might outlive the object itself.

These are useful for asynchronous work, which is common in Chrome. If an async
task references other objects or state, and it's possible for that state to be
destroyed before the task runs, those references should be held in a
`WeakPtr<T>`. Each `WeakPtr<T>` is associated with a `WeakPtrFactory<T>`. When
the associated factory (usually owned by T) is destroyed, all `WeakPtr<T>` are
invalidated (becomes null) rather than becoming use-after-frees. If such
references should never outlive the object, consider using SafeRef instead.

## `base::SafeRef<T>`
Use to express that a reference to an object must not outlive the object.

An example is if you have a class member that you want to guarantee outlives the
class itself. SafeRef automatically enforces the lifetime assumptions and
eliminates the need for validity checks.

If the assumption that the object is valid is broken, then the process
terminates safely and generates a crash report. Though not ideal, it's
preferable to a potentially undiscovered security bug.

This type is built on top of WeakPtr, so if you want a `SafeRef<T>`, T needs a
WeakPtrFactory as a member. It works like `WeakPtr`, but doesn't allow for a
null state. There's also overlap with `raw_ptr`, though this was implemented
first.

## `scoped_refptr<T>`
Use when you want manually managed strong refcounting. Use carefully!

It’s an owning smart pointer, so it owns a pointer to something allocated in the
heap and gives shared ownership of the underlying object, since it can be
copied. When all `scoped_refptr<T>`s pointing to the same object are gone, that
object gets destroyed.

This is Chrome's answer to `std::shared_ptr<T>`. It additionally requires T to
inherit from `RefCounted` or `RefCountedThreadSafe`, since the ref counting
happens in the object itself, unlike `shared_ptr<T>`.

It's preferred for an object to remain on the same thread, as `RefCounted` is
much cheaper. If there are `scoped_refptr<T>`s to the same object on different
threads, use `RefCountedThreadSafe`, since accesses to the reference count can
race. In this case, without external synchronization, the destructor of
`scoped_refptr<T>`, which decreases the reference count by one, can run on any
thread.

Inheriting from `RefCountedThreadSafe` by itself doesn't make a class `T` or the
underlying object of `scoped_refptr<T>` thread-safe: It merely ensures that the
counter manipulated by `scoped_refptr<T>` is thread-safe.

If the destructor interacts with other systems it is important to
control and know which thread has the last reference to the object, or you can
end up with flakiness.
