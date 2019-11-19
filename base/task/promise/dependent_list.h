// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_DEPENDENT_LIST_H_
#define BASE_TASK_PROMISE_DEPENDENT_LIST_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace base {
namespace internal {

// Returns 2^N where N is the smallest N possible so that 2^N > value.
constexpr uintptr_t NextPowerOfTwo(uintptr_t value) {
  // Keep setting 1's to the right of the first one until there are only 1's. In
  // each iteration we double the number of 1's that we set. At last add 1 and
  // we have the next power of 2.
  for (size_t i = 1; i < sizeof(uintptr_t) * 8; i <<= 1) {
    value |= value >> i;
  }
  return value + 1;
}

class AbstractPromise;

// AbstractPromise needs to know which promises depend upon it. This lock free
// class stores the list of dependents. This is not a general purpose list
// because the data can only be consumed once.
//
// This class is thread safe.
class BASE_EXPORT DependentList {
 public:
  struct ConstructUnresolved {};
  struct ConstructResolved {};
  struct ConstructRejected {};

  explicit DependentList(ConstructUnresolved);
  explicit DependentList(ConstructResolved);
  explicit DependentList(ConstructRejected);

  ~DependentList();

  DependentList(const DependentList&) = delete;
  DependentList& operator=(const DependentList&) = delete;

  enum class InsertResult {
    SUCCESS,
    FAIL_PROMISE_RESOLVED,
    FAIL_PROMISE_REJECTED,
    FAIL_PROMISE_CANCELED,
  };

  class BASE_EXPORT ALIGNAS(8) Node {
   public:
    Node();
    explicit Node(Node&& other) noexcept;

    // Constructs a Node, |prerequisite| will not be retained unless
    // RetainSettledPrerequisite is called.
    Node(AbstractPromise* prerequisite,
         scoped_refptr<AbstractPromise> dependent);
    ~Node();

    // Caution this is not thread safe.
    void Reset(AbstractPromise* prerequisite,
               scoped_refptr<AbstractPromise> dependent);

    // Expected prerequisite usage:
    // 1. prerequisite = null on creation (or is constructed with a value)
    // 2. (optional, once only) SetPrerequisite(value)
    // 3. (maybe, once only) RetainSettledPrerequisite();
    // 4. (maybe) ClearPrerequisite()
    // 5. Destructor called

    // Can be called on any thread.
    void SetPrerequisite(AbstractPromise* prerequisite);

    // Can be called on any thread.
    AbstractPromise* prerequisite() const;

    scoped_refptr<AbstractPromise>& dependent() { return dependent_; }

    const scoped_refptr<AbstractPromise>& dependent() const {
      return dependent_;
    }

    Node* next() const { return next_; }

    // Calls AddRef on |prerequisite()| and marks the prerequisite as being
    // retained. The |prerequisite()| will be released by Node's destructor or
    // a call to ClearPrerequisite. Does nothing if called more than once.
    // Can be called on any thread at any time. Can be called once only.
    void RetainSettledPrerequisite();

    // Calls Release() if the rerequsite was retained and then sets
    // |prerequisite_| to zero. Can be called on any thread at any time. Can be
    // called more than once.
    void ClearPrerequisite();

   private:
    friend class DependentList;

    void MarkAsRetained() { prerequisite_ |= kIsRetained; }

    // An AbstractPromise* where the LSB is a flag which specified if it's
    // retained or not.
    // A reference for |prerequisite_| is acquired with an explicit call to
    // AddRef() if it's resolved or rejected.
    std::atomic<intptr_t> prerequisite_{0};

    scoped_refptr<AbstractPromise> dependent_;
    Node* next_ = nullptr;

    static constexpr intptr_t kIsRetained = 1;
  };

  // Insert will only succeed if neither ResolveAndConsumeAllDependents nor
  // RejectAndConsumeAllDependents nor CancelAndConsumeAllDependents have been
  // called yet. If the call succeeds, |node| must remain valid pointer until it
  // is consumed by one of the *AndConsumeAllDependents methods. If none of
  // those methods is called |node| must only be valid for the duration of this
  // call. Nodes will be consumed in the same order as they are inserted.
  InsertResult Insert(Node* node);

  // Callback for *AndConsumeAllDependents methods.
  // TODO(carlscab): Consider using a callable object instead.
  class BASE_EXPORT Visitor {
   public:
    virtual ~Visitor();
    // Called from the *AndConsumeAllDependents methods for each node.
    // |dependent| is the consumed (i.e. moved) from the one associated with the
    // node. It is fine if the pointer to the node becomes invalid inside this
    // call (i.e it is fine to delete the node).
    virtual void Visit(scoped_refptr<AbstractPromise> dependent) = 0;
  };

  // The following *AndConsumeAllDependents methods will settle the list and
  // consume all previously inserted nodes. It is guaranteed that Insert()
  // failures will happen-after all nodes have been consumed. In particular that
  // means that if an Insert happens while we are still consuming nodes the
  // Insert will succeed and the node will be appended to the list of nodes to
  // consume and eventually be consumed.
  //
  // ATTENTION: Calls to any of this methods will fail if itself or a different
  // consume method has been previously called. ResolveAndConsumeAllDependents
  // and RejectAndConsumeAllDependents will DCHECK on failures and
  // CancelAndConsumeAllDependents will return false if it fails.

  void ResolveAndConsumeAllDependents(Visitor* visitor) {
    const bool success =
        SettleAndDispatchAllDependents(State::kResolved, visitor);
    DCHECK(success) << "Was already settled";
  }

  void RejectAndConsumeAllDependents(Visitor* visitor) {
    const bool success =
        SettleAndDispatchAllDependents(State::kRejected, visitor);
    DCHECK(success) << "Was already settled";
  }

  // TODO(alexclarke): Consider DCHECK for failures which would also allow us to
  // greatly simplify SettleAndDispatchAllDependents
  bool CancelAndConsumeAllDependents(Visitor* visitor) {
    return SettleAndDispatchAllDependents(State::kCanceled, visitor);
  }

  // Returns true if any of IsResolved, IsRejected, or IsCanceled would return
  // true
  bool IsSettled() const;

  // Returns true if (Resolve/Reject/Cancel)AndConsumeAllDependents
  // has resolved/rejected/canceled the promise, respectively.
  //
  // ATTENTION: No guarantees are made as of whether the
  // (Resolve/Reject/Cancel)AndConsumeAllDependents method is still executing.
  bool IsCanceled() const;

  // DCHECKs if not settled.
  bool IsResolved() const;

  // DCHECKs if not settled.
  bool IsRejected() const;

  // Like the above but doesn't DCHECK if unsettled.
  bool IsResolvedForTesting() const;
  bool IsRejectedForTesting() const;

 private:
  // The data for this class is:
  //   * head: Pointer to the head of the list of Node instances
  //   * allow_inserts: flag indicating whether further inserts are allowed
  //   * state: State value
  //
  // We store all this information in a uintptr_t to support atomic operations
  // as follows:
  // PP...PPPFSS
  //   * P: Pointer to the head of the list of Node instances (head)
  //   * F: Flag inidicating whether further inserts are allowed (allow_inserts)
  //   * S: State value (state)
  //
  // The various *Mask constants contain the bit masks for the various fields.
  //
  // Inserts can be allowed in any of the states, but they MUST be allowed in
  // State::kUnresolved. Inserts are allowed while in one of the settled states
  // while the SettleAndDispatchAllDependents is dispatching nodes. This is done
  // so to preserve dispatch order. Once all nodes have been dispatched (i.e.
  // the list is empty), the allow_inserts is atomically (making sure list is
  // still empty) set to false. From that point on Inserts will fail.
  //
  // All valid state transitions start from State::kUnresolved i.e. only the
  // first call to SettleAndDispatchAllDependents will be able to settle the
  // state and succeed, all others will fail.
  //
  // The Is(Resolved|Rejected|Canceled) methods must return true while we are
  // dispatching nodes. That is we need to access the settled state while we are
  // still dispatching nodes. Thus we need and extra bit (allow_inserts) so that
  // Insert can determine whether to insert or fail when there is a settled
  // state.

  enum class InsertPolicy {
    kAllow,
    kBlock,
  };
  static constexpr auto kAllowInserts = InsertPolicy::kAllow;
  static constexpr auto kBlockInserts = InsertPolicy::kBlock;

  enum class State {
    kUnresolved = 0,
    kResolved,
    kRejected,
    kCanceled,
    kLastValue = kCanceled
  };

  static constexpr uintptr_t kStateMask =
      NextPowerOfTwo(static_cast<uintptr_t>(State::kLastValue)) - 1;
  static constexpr uintptr_t kAllowInsertsBitMask = kStateMask + 1;
  static constexpr uintptr_t kHeadMask = ~(kAllowInsertsBitMask | kStateMask);
  static constexpr uintptr_t kRequiredNodeAlignment = ~kHeadMask + 1;

  // It is really important that Node instances get aligned so that we can store
  // state in the lower bits of pointers.
  //
  // Allocators are required to return memory aligned at least as strictly as
  // std::max_align_t but not more, so we can not ask for a bigger alignment
  // here otherwise we risk not getting proper alignment from heap allocations.
  // Also, to make sure that stack allocations also get properly aligned (used
  // currently in tests) we need the ALIGNAS(8) attribute, which is ignored by
  // allocators. Thus we need the two following static_asserts.
  static_assert(
      std::alignment_of<Node>() >= kRequiredNodeAlignment,
      "Will not be able to hold the Node* and all the state in a uintptr_t");
  static_assert(
      std::alignment_of<std::max_align_t>() >= std::alignment_of<Node>(),
      "malloc (et al.) will not return memory propery aligned to be able to "
      "hold the Node* and all the state in a uintptr_t");

  static State ExtractState(uintptr_t data) {
    return static_cast<State>(data & kStateMask);
  }

  static DependentList::Node* ExtractHead(uintptr_t data) {
    return reinterpret_cast<DependentList::Node*>(data & kHeadMask);
  }

  static bool IsListEmpty(uintptr_t data) {
    return ExtractHead(data) == nullptr;
  }

  static bool IsAllowingInserts(uintptr_t data) {
    return data & kAllowInsertsBitMask;
  }

  static uintptr_t CreateData(Node* head,
                              State state,
                              InsertPolicy insert_policy) {
    DCHECK_EQ(uintptr_t(head), uintptr_t(head) & kHeadMask)
        << "Node doesn't have enough alignment";
    DCHECK(insert_policy == kAllowInserts || head == nullptr)
        << "List must be empty if no more inserts are allowed";
    DCHECK(insert_policy == kAllowInserts || state != State::kUnresolved)
        << "Can not block inserts and remain in kUnresolved state";
    return reinterpret_cast<uintptr_t>(head) |
           (insert_policy == kAllowInserts ? kAllowInsertsBitMask : 0) |
           (static_cast<uintptr_t>(state) & kStateMask);
  }

  explicit DependentList(State initial_state);

  // Settles the list and consumes all previously inserted nodes. If the list is
  // already settled it does nothing and returns false, true otherwise.
  bool SettleAndDispatchAllDependents(State settled_state, Visitor* visitor);

  static DependentList::Node* ReverseList(DependentList::Node* list);

  // Goes through the list starting at |head| consuming node->dependent and
  // passing it to the provided |visitor|.
  static void DispatchAll(DependentList::Node* head,
                          DependentList::Visitor* visitor,
                          bool retain_prerequsites);

  std::atomic<uintptr_t> data_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_DEPENDENT_LIST_H_
