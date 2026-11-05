# Code Review Pitfalls

Review heuristics for Chromium code that are not covered by the canonical docs.
For IPC and Mojo, first read `docs/security/mojo.md` (trusting less privileged
processes, structured types, `ReportBadMessage` vs `CHECK`,
`GetBadMessageCallback` after async hops, `StructTraits::Read()` validation,
copying out of `BigBuffer`, WebUI binding grants, `EnableIf`) and
`docs/security/origin-vs-url.md` (use the last committed origin, not URLs, for
security decisions). The items below complement those docs.

## Release-Build vs Debug-Build Constructs

- **`NOTREACHED()` is `[[noreturn]]` and release-fatal since M129.** Any claim
  that execution continues past `NOTREACHED()` is invalid — the process
  terminates. Do not report code paths past `NOTREACHED()` as reachable.
- **`SEQUENCE_CHECKER` / `THREAD_CHECKER` / `DCHECK_CALLED_ON_VALID_*` provide
  zero release-build protection.** These are debug-only assertions. A class
  whose only thread-safety check is one of these macros has a real data race in
  release builds.
- **`DCHECK` / `DLOG(DFATAL)` are stripped from release builds.** Conditions
  guarded only by `DCHECK` are fully reachable in release builds.

## Memory Safety Mitigations

- **`RAW_PTR_EXCLUSION`:** Fields annotated with this attribute are explicitly
  excluded from MiraclePtr protection and may be exploitable. Check whether
  suspicious member pointers are `raw_ptr<T>` (protected) or raw C++
  pointers/references (exploitable).
- **`Unretained()` dangling pointers:** May be mitigated-to-crash by
  `kPartitionAllocUnretainedDanglingPtr`. Check feature status before rating
  severity — if enabled, the dangling pointer triggers an immediate safe crash
  rather than exploitable memory corruption.
- **Member pointers should be `raw_ptr<T>` / `raw_ref<T>`.** A new class member
  that holds a native C++ pointer or reference to a heap object forgoes
  MiraclePtr protection. Recommend `raw_ptr<T>` (or `raw_ref<T>` for non-null
  references); plain `T*` / `const T&` remain appropriate for function
  parameters and locals.
- **`LIFETIME_BOUND` on accessors returning references to member state.**
  Accessors that return a reference or view (`std::string_view`, `base::span`)
  into an object's own storage should be annotated with `LIFETIME_BOUND` so the
  compiler rejects dangling uses on temporaries.

## Pointer and Reference Safety

- **`WeakPtr` is only safe on its bound sequence.** Cross-sequence
  `if (wp) wp->...` is a TOCTOU race — the weak pointer can be invalidated
  between the check and the dereference. `WeakPtrFactory` must be the last
  declared member variable so it is destroyed first.
- **`RefCounted<T>` (without `ThreadSafe`) has a non-atomic refcount.** Copying
  or destroying a `scoped_refptr` off the origin thread is a race. Only
  `ThreadSafeRefCounted<T>` is safe for cross-thread use.
- **Oilpan (Blink GC):** Every `Member<T>` / `WeakMember<T>` field must appear
  in `Trace()`. An untraced member is invisible to the garbage collector and
  becomes a UAF after the next GC cycle.
- **`DowncastTraits<T>::AllowFrom()` is the type check behind `To<T>()`.** Audit
  the `AllowFrom()` predicate, not the `To<T>()` call site — an incorrect
  predicate allows type confusion.

## Browser-Side Trust and IPC

- **Comparing child data with child data is no check.** A browser-side check
  that compares renderer-supplied data against other renderer-supplied data
  provides no protection. Decisions must rely on authoritative browser-side
  state (e.g. `RenderFrameHost::GetLastCommittedOrigin()`, `GetProcess()`,
  `ChildProcessSecurityPolicy`).
- **When early return is acceptable.** Because state can be inconsistent across
  processes, explicit state handling (for example checking
  `RenderFrameHost::IsActive()` and returning if it is no longer the active one)
  may legitimately return without reporting a bad message.
- **Filesystem brokerage.** Untrusted child processes must never dictate
  arbitrary filesystem paths for the browser to read, write, copy, or delete.
  Paths must be chosen authoritatively by the browser, or the browser must hand
  down a pre-opened `base::File` with the minimal set of flags.
- **Data disclosure in the browser-to-child direction.** Check what the browser
  sends down. Payloads to a renderer must not contain cross-origin data, data
  belonging to another profile or storage partition, or privileged internal
  state the renderer has no need for.
- **Enumerated choices should be Mojo `enum`s.** Beyond the structured types
  listed in `docs/security/mojo.md`, enumerated choices and identifiers should
  be strongly-typed Mojo `enum`s rather than raw integers or free-form strings,
  so that validation is not pushed into ad-hoc parsing on the privileged side.
- **Generated validation only checks structure.** The generated Mojo
  `Validate()` method only checks structural layout. If `StructTraits::Read()`
  returns `true` without verifying enum ranges, length consistency, or origin
  legitimacy, the browser receives a trusted type with attacker-controlled
  contents.
- **Prefer read-only shared memory.** When the browser shares memory with a
  child process, use `base::ReadOnlySharedMemoryRegion` unless the child must
  write. Writable regions remain mutable by the child after receipt;
  validate-then-use is a TOCTOU vulnerability unless data is copied out first.
- **Synchronous IPC (`[Sync]`):** New synchronous Mojo calls in the browser or
  renderer are prohibited without explicit IPC security justification. Sync IPCs
  block the calling thread, create re-entrancy and deadlock surfaces, and let a
  child process stall the browser. Treat a new `[Sync]` attribute as a finding
  unless its justification is documented.
- **Interface scoping:** Capabilities tied to a document or frame must be
  exposed through document-scoped interfaces (e.g. `DocumentService<T>` or
  frame-associated interfaces) rather than process-wide interfaces. A
  process-global interface that accepts a routing ID or frame token from the
  renderer to select the target is a confused-deputy pattern: the browser must
  verify that the calling process is allowed to act on that frame.
- **Extensible enums (`[Extensible] enum`):** Enums marked `[Extensible]` in
  `.mojom` files do NOT receive auto-generated range checks during
  deserialization. Handlers must explicitly validate that values fall within
  known ranges, handle unexpected values safely, and NEVER use unvalidated
  extensible enum values directly as array/vector indices or buffer offsets.
- **`PickleIterator::Read*()` returns `bool`; ignoring the return value reads
  uninitialized stack memory.** Every `Read*()` call must be checked.

## Bounds and Memory

- **`base::span` indexing (`operator[]`) is CHECKed, but `.data()` arithmetic is
  not.** Raw pointer arithmetic via `.data()` bypasses bounds checks.
- **Raw pointer + length parameter pairs are a finding in new code.** Buffer
  views should be `base::span<const uint8_t>` (or the appropriate element type),
  constructed with helpers such as `base::as_byte_span()`,
  `base::byte_span_from_ref()`, or `base::span_from_cstring()`. A new
  `(const uint8_t* data, size_t len)` signature reintroduces the unchecked
  arithmetic that spanification removes.
- **`UNSAFE_BUFFERS(...)` requires a `// SAFETY:` comment.** Every use must be
  immediately preceded by a `// SAFETY:` rationale explaining why the bounds are
  guaranteed. A missing rationale is a finding; a rationale that does not
  actually hold (for example, it relies on a length supplied by a child process)
  is a memory-safety finding.

## Navigation and Document Lifecycle

Navigation and frame lifecycle bugs are a recurring source of browser-process
UAFs and confused-deputy issues. For changes in this area, consult
`docs/navigation.md`, `docs/navigation_concepts.md`, and
`docs/special_case_urls.md`.

- **Asynchronous IPC vs synchronous teardown.** For every asynchronous message
  or callback from a renderer, ask what happens if it races with a synchronous
  frame detach, navigation cancellation, or window close in the browser. Objects
  captured before the hop may be gone by the time the task runs.
- **Navigating away does not mean destruction.** A document may be stored in the
  back/forward cache or may be a prerendered page that is later activated. Code
  that assumes "not current" means "destroyed" (or vice versa) must check
  `RenderFrameHost::IsActive()` / `GetLifecycleState()` and handle restoration
  and activation correctly.
- **Frame hierarchy nuances.** Account for inner `WebContents` (e.g.
  GuestViews), and other embedded frame trees. "Main frame" is not the same as
  "primary main frame" or "outermost main frame"; use `IsInPrimaryMainFrame()`
  or `IsInOutermostMainFrame()` and related helpers where the distinction
  matters for the security decision.
- **Document-scoped state must be cleaned up with the document.** State that
  belongs 1:1 to a document should live in `DocumentUserData<T>` or
  `DocumentService<T>`; page-level state in `PageUserData<T>`; in-flight
  navigation state in `NavigationHandleUserData<T>`. Ad-hoc maps keyed by
  `RenderFrameHost*` or state kept in a `WebContentsObserver` outlive the
  document they describe and are a common source of stale-pointer and
  cross-document confusion bugs.

## Sandbox Type Selection

- **Least privilege for new processes and services.** Out-of-process services
  and utility processes must select the most restrictive
  `sandbox::mojom::Sandbox` that lets them function. `Sandbox::kNoSandbox` is a
  finding unless it has been explicitly approved by the sandbox owners and the
  justification is recorded. See
  `docs/security/process-sandboxes-by-platform.md` for which sandboxes apply on
  which platforms.
