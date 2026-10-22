// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_LINKED_LIST_H_
#define BASE_CONTAINERS_LINKED_LIST_H_

#include <utility>

#include "base/base_export.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr_exclusion.h"

// Simple linked list type. (See the Q&A section to understand how this
// differs from std::list).
//
// To use, start by declaring the class which will be contained in the linked
// list, as extending UnsafeLinkNode or SafeLinkedNode (this gives it
// next/previous pointers).
//
//   class MyNodeType : public UnsafeLinkNode<MyNodeType> {
//     ...
//   };
//
// Next, to keep track of the list's head/tail, use an UnsafeLinkedList or
// SafeLinkedList instance:
//
//   UnsafeLinkedList<MyNodeType> list;
//
// To add elements to the list, use any of Append, InsertBefore, or InsertAfter:
//
//   MyNodeType* n1 = ...;
//   MyNodeType* n2 = ...;
//   MyNodeType* n3 = ...;
//
//   list.Append(n1);
//   list.Append(n3);
//   n2->InsertBefore(n3);
//
// Lastly, to iterate through the linked list forwards:
//
//   for (auto* node = list.head();
//        node != list.end();
//        node = node->next()) {
//     MyNodeType* value = node->value();
//     ...
//   }
//
// Or to iterate the linked list backwards:
//
//   for (auto* node = list.tail();
//        node != list.end();
//        node = node->previous()) {
//     MyNodeType* value = node->value();
//     ...
//   }
//
// Questions and Answers:
//
// Q. Should I use std::list or base::(Safe|Unsafe)LinkedList?
//
// A. The main reason to use base::(Safe|Unsafe)LinkedList over std::list is
//    performance. If you don't care about the performance differences
//    then use an STL container, as it makes for better code readability.
//
//    Comparing the performance of base::UnsafeLinkedList<T> to std::list<T*>:
//
//    * Erasing an element of type T* from base::UnsafeLinkedList<T> is
//      an O(1) operation. Whereas for std::list<T*> it is O(n).
//      That is because with std::list<T*> you must obtain an
//      iterator to the T* element before you can call erase(iterator).
//
//    * Insertion operations with base::UnsafeLinkedList<T> never require
//      heap allocations.
//
// Q. How does base::(Safe|Unsafe)LinkedList implementation differ from
//    std::list?
//
// A. Doubly-linked lists are made up of nodes that contain "next" and
//    "previous" pointers that reference other nodes in the list.
//
//    With base::(Safe|Unsafe)LinkedList<T>, the type being inserted already
//    reserves space for the "next" and "previous" pointers
//    (base::(Safe|Unsafe)LinkNode<T>*). Whereas with std::list<T> the type can
//    be anything, so the implementation needs to glue on the "next" and
//    "previous" pointers using some internal node type.
//
// Q. What is the difference between the "Safe" and "Unsafe" versions of the
//    list and node?
//
// A. UnsafeLinkNode and UnsafeLinkedList allows you to do bulk allocation and
//    deletion of a list and all of its nodes without having to individually
//    remove the nodes from the list (they all have trivial destructors), so
//    they're more efficient for some applications.
//
//    However, a node can be deleted without being removed from the list, which
//    may introduce use-after-free bugs. Likewise a list may be deleted before
//    its nodes, which would allow a freed root node to be accessed through
//    UnsafeLinkNode::next() or UnsafeLinkNode::previous().
//
//    SafeLinkNode and SafeLinkedList guard against accessing dead nodes,
//    by CHECKing that a node has been removed from the list on destruction, and
//    similarly, that all lists are empty on destruction. This comes with some
//    destructor-time overhead, so it's a tradeoff between performance and
//    safety.
//
//    In general, if you are block-allocating and deleting nodes and lists
//    together and performance is paramount, use UnsafeLinkedList; if you are
//    dynamically creating and destroying nodes, SafeLinkedList is better.

namespace base {

namespace internal {

template <typename Node>
class LinkedListBaseT;

// Base class for UnsafeLinkNode<T> and SafeLinkNode<T>.
class BASE_EXPORT LinkNodeBase {
 public:
  void RemoveFromList();

  LinkNodeBase(const LinkNodeBase&) = delete;
  LinkNodeBase& operator=(const LinkNodeBase&) = delete;

 protected:
  LinkNodeBase();
  LinkNodeBase(LinkNodeBase* previous, LinkNodeBase* next);
  LinkNodeBase(LinkNodeBase&& rhs);
  ~LinkNodeBase() = default;

  // Calling these with |e| as a different link node type as |this| is
  // unsafe. These are protected and only called from the derived template node
  // classes to ensure safety.
  void InsertBeforeBase(LinkNodeBase* e);
  void InsertAfterBase(LinkNodeBase* e);

  LinkNodeBase* previous_base() const { return previous_; }
  LinkNodeBase* next_base() const { return next_; }

 private:
  template <typename Node>
  friend class ::base::internal::LinkedListBaseT;

  // Make `previous_` and `next_` point to `this`. Can only be called when
  // `next_` and `previous_` are nullptr or already point to `this`.
  void MakeSelfReferencing();

  // `previous_` and `next_` are not a raw_ptr<...> for performance reasons:
  // on-stack pointer + a large number of non-PA pointees through WeakLinkNode +
  // based on analysis of sampling profiler data and tab_search:top100:2020.
  RAW_PTR_EXCLUSION LinkNodeBase* previous_ = nullptr;
  RAW_PTR_EXCLUSION LinkNodeBase* next_ = nullptr;
};

// Base type for both safe and unsafe link nodes. `T` is the node type, and
// `Node` is the base node type.
template <typename T, typename Node>
class LinkNodeBaseT : public internal::LinkNodeBase {
 public:
  // Insert |this| into the linked list, before |e|. |this| must not
  // already be in a list.
  void InsertBefore(Node* e) { InsertBeforeBase(e); }

  // Insert |this| into the linked list, after |e|. |this| must not
  // already be in a list.
  void InsertAfter(Node* e) { InsertAfterBase(e); }

  Node* previous() const { return static_cast<Node*>(this->previous_base()); }

  Node* next() const { return static_cast<Node*>(this->next_base()); }

  // Cast from the node-type to the value type.
  const T* value() const { return static_cast<const T*>(this); }
  T* value() { return static_cast<T*>(this); }

 protected:
  LinkNodeBaseT() = default;
  LinkNodeBaseT(Node* previous, Node* next)
      : internal::LinkNodeBase(previous, next) {}
  LinkNodeBaseT(LinkNodeBaseT<T, Node>&&) = default;
  ~LinkNodeBaseT() = default;
};

// Base class for linked lists. `Node` is the base node type (either LinkedNode
// or SafeLinkedNode).
template <typename Node>
class LinkedListBaseT {
 public:
  LinkedListBaseT(const LinkedListBaseT&) = delete;
  LinkedListBaseT& operator=(const LinkedListBaseT&) = delete;

  // Appends |e| to the end of the linked list.
  void Append(Node* e) { e->InsertBefore(&root_); }

  Node* head() const { return root_.next(); }

  Node* tail() const { return root_.previous(); }

  const Node* end() const { return &root_; }

  bool empty() const { return head() == end(); }

 protected:
  // The "root" node is self-referential, and forms the basis of a circular
  // list (root_.next() will point back to the start of the list,
  // and root_->previous() wraps around to the end of the list).
  LinkedListBaseT() : root_(&root_, &root_) {}

  // Use move constructor with care. Returning a linked list from a function may
  // be unsafe if the nodes are allocated on the stack. This operation is O(1)
  // as only head and tail nodes are modified. `other` is left empty.
  LinkedListBaseT(LinkedListBaseT&& other) : root_(std::move(other.root_)) {
    other.root_.MakeSelfReferencing();
  }

  ~LinkedListBaseT() = default;

  Node root_;
};

}  // namespace internal

// Basic node in a UnsafeLinkedList<T>.
//
// This implementation is fast but provides no UAF protection if a node is
// destroyed without removing it from a list first. If you want protection (at
// the cost of some usage constraints) use SafeLinkNode/SafeLinkedList instead.
template <typename T>
class UnsafeLinkNode : public internal::LinkNodeBaseT<T, UnsafeLinkNode<T>> {
 public:
  UnsafeLinkNode() = default;
  UnsafeLinkNode(UnsafeLinkNode<T>* previous, UnsafeLinkNode<T>* next)
      : internal::LinkNodeBaseT<T, UnsafeLinkNode<T>>(previous, next) {}
  UnsafeLinkNode(UnsafeLinkNode<T>&&) = default;
};

// Basic linked list without any UAF protection.
template <typename T>
class UnsafeLinkedList : public internal::LinkedListBaseT<UnsafeLinkNode<T>> {
 public:
  using NodeType = T;

  UnsafeLinkedList() = default;
  UnsafeLinkedList(UnsafeLinkedList&& other) = default;
  ~UnsafeLinkedList() = default;
};

// TODO(https://crbug.com/571500505): eliminate this once all instances are
// renamed.
template <typename T>
using LinkNode = UnsafeLinkNode<T>;
template <typename T>
using LinkedList = UnsafeLinkedList<T>;

// Linked list node for use in a SafeLinkedList<T>.
//
// Use this if you want to ensure that "dead" nodes are never left in a list,
// avoiding potential UAFs (at a small additional cost per node on destruction).
template <typename T>
class SafeLinkNode : public internal::LinkNodeBaseT<T, SafeLinkNode<T>> {
 public:
  SafeLinkNode() = default;
  SafeLinkNode(SafeLinkNode<T>* previous, SafeLinkNode<T>* next)
      : internal::LinkNodeBaseT<T, SafeLinkNode<T>>(previous, next) {}
  SafeLinkNode(SafeLinkNode<T>&&) = default;
  ~SafeLinkNode() {
    // A self-referencing node should always be the list root, and is destroyed
    // with the list itself.
    if (this->next_base() != this || this->previous_base() != this) {
      CHECK_EQ(nullptr, this->next_base());
      CHECK_EQ(nullptr, this->previous_base());
    }
  }
};

// Linked list for use with SafeLinkNode. Guarantees that no dead node will ever
// be reachable from a live node, no matter what order nodes or lists are
// destroyed in.
template <typename T>
class SafeLinkedList : public internal::LinkedListBaseT<SafeLinkNode<T>> {
 public:
  using NodeType = T;

  SafeLinkedList() = default;
  SafeLinkedList(SafeLinkedList&& other) = default;
  ~SafeLinkedList() { CHECK(this->empty()); }

  // Removes all nodes from this list. Useful if you intend to destroy your list
  // before some or all of its nodes. (Nodes may be reused.)
  void clear() {
    auto* const root = this->end();
    while (!this->empty()) {
      root->next()->RemoveFromList();
    }
  }
};

}  // namespace base

#endif  // BASE_CONTAINERS_LINKED_LIST_H_
