---
name: public-transit
description: >-
  Use when writing, migrating, or reviewing Android instrumentation tests
  (javatests) or Public Transit code (Stations, Facilities, Outcomes,
  Conditions, ViewFinder, hopOnAt/hopOnTo), or debugging TravelException
  failures and timeouts.
---

# Public Transit

Documentation for Chromium Android's Public Transit framework lives alongside
the code under
`base/test/android/javatests/src/org/chromium/base/test/transit/`. Read the
files relevant to the task before making or reviewing changes:

- **[`README.md`](/base/test/android/javatests/src/org/chromium/base/test/transit/README.md)**:
  read for API details and core concepts — `Station`, `Facility`, `State`,
  `Element`, `Condition`, `Transition`, `ViewFinder`, batching rules, and
  hopping on/off.
- **[`best_practices.md`](/base/test/android/javatests/src/org/chromium/base/test/transit/best_practices.md)**:
  read when authoring, migrating, or reviewing tests and Transit Layer code —
  layer separation, naming, `Outcome`, element scoping, `WebPageFacility`,
  `inNewTask()`, `*Fast()` helpers, `ViewFinder`, and hop-on test-util
  conversions. When reviewing, check surrounding callers and parent `Station` /
  `Facility` classes as well, and avoid pushing unnecessary boilerplate (such as
  `Outcome` on single-trigger trips or getters for rarely read elements).
- **[`debugging.md`](/base/test/android/javatests/src/org/chromium/base/test/transit/debugging.md)**:
  read before changing a timeout or debugging a failing or flaky test —
  `ViewPrinter`, `PublicTransitConfig`, `ConditionWaiter` timeout defaults,
  reading `[OK]` timestamps, and tracing `[BLKD]` conditions upstream.
- **Existing Transit Layer packages**: inspect before adding a new `Station`,
  `Facility`, `Condition`, trigger (`*To()`), or `Outcome` so you reuse or
  extend existing code instead of duplicating it:
  - `chrome/test/android/javatests/src/org/chromium/chrome/test/transit/`
  - `content/public/test/android/javatests/src/org/chromium/content_public/browser/test/transit/`
  - `base/test/android/javatests/src/org/chromium/base/test/transit/`
- **[`android-test-batching`](/agents/skills/android-test-batching/SKILL.md)**:
  read alongside this skill when auditing `@Batch` isolation or cleaning up
  state bleed between batched tests.
