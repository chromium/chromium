# Android Tab Architecture & Persistence (`chrome/browser/tab/`)

> **Maintenance Notice:** This file is the central documentation hub for `chrome/browser/tab/`, `chrome/android/java/src/org/chromium/chrome/browser/tab/` ([`GEMINI.md`](/chrome/android/java/src/org/chromium/chrome/browser/tab/GEMINI.md)), and `chrome/browser/android/tab_*`. Update this file whenever any documented behavior, contract, or directory structure changes.

---

## 1. Ecosystem & Directory Structure

The Android Tab subsystem is split across three locations to enforce strict modularization boundaries while bridging Clank's Java `Tab` hierarchy with cross-platform desktop C++ `tabs::TabInterface` and `tabs::TabStripCollection` abstractions.

### Modularization & `DEPS` Boundary
- `chrome/browser/tab/` should not depend on `chrome/android/`; however, the inverse is allowed.
- All interactions with browser-level UI or activities are inverted via interfaces and handlers defined in `chrome/browser/tab/` (`TabDelegateFactory`, `TabResolver`, `TabObscuringHandler`) and implemented or supplied by `chrome/android/`.

### Directory Layout & Responsibilities

```text
chrome/browser/tab/
├── *.{h,cc}                        # C++ WebContentsState serialization & TabStateStorageService engine
├── protocol/                       # Protobuf schemas (tabs_pb) for tab & TabCollection node persistence
└── java/.../browser/tab/           # Modularized public Java Tab APIs & storage bridges (noparent DEPS)
    └── state/                      # PersistedTabData framework & state/proto/ schemas

chrome/android/java/.../browser/tab/ # Concrete chrome_java TabImpl, TabBuilder, view glue & archiving
└── tab_restore/                    # Historical tab saving ("Recently closed") integration

chrome/browser/android/             # Native TabAndroid (tabs::TabInterface), TabFeatures & JNI bridges
```

- **`chrome/browser/tab/` (C++ code & SQLite Persistence Engine)**:
  - **`WebContentsState` Serialization**: Serializes and deserializes `content::WebContents` navigation histories into `base::Pickle` / direct JNI `ByteBuffer` (`WebContentsStateByteBuffer`), extracts display metadata without full unpickling, and synthesizes or appends pending navigations directly into pickled buffers.
  - **`TabStateStorageService` Engine**: KeyedService and SQLite backend that persists the hierarchical `tabs::TabStripCollection` tree (`TabInterface`, pinned/unpinned collections, tab groups, split tabs) as graph nodes (`nodes` and `divergent_nodes` tables, with window/strip metadata stored on root `kTabStrip` nodes). Includes UI-thread state packagers, live `TabStripCollection` mutation synchronizers, an incremental session restore orchestrator that reconciles in-flight restores against live user mutations, AEAD cryptographic sealing for off-the-record state, and `ScopedBatch` write batching.
- **`chrome/browser/tab/protocol/` (Protobuf Schemas — `tabs_pb`)**:
  - Protobuf definitions for persisting individual tab state, child ordering, tokens, and `TabCollection` node states (`TabStripCollection`, `TabGroupTabCollection`, `SplitTabCollection`).
- **`chrome/browser/tab/java/` (Modularized Public Java API)**:
  - Public interfaces and contracts (`Tab`, `TabObserver`, `TabDelegateFactory`, `TabViewManager`, `TabWebContentsDelegateAndroid`), serialized tab state (`TabState`, `WebContentsState`, `TabStateAttributes`), per-tab `UserDataHost` / `TabWebContentsUserData` containers, and Java bridges for `TabStateStorageService`.
  - **`state/` Subpackage**: The asynchronous `PersistedTabData` key-value framework and its accompanying protobuf schemas (`state/proto/`).
- **`chrome/android/java/src/org/chromium/chrome/browser/tab/` (Concrete Implementation & Browser Glue)**:
  - Monolithic `chrome_java` implementation containing package-private `TabImpl`, public `TabBuilder`, `TabHelpers`, `WebContents` and Android view integration glue, tab archiving, and `tab_restore/` for recording closed tabs/groups into `sessions::TabRestoreService`.
- **`chrome/browser/android/` (Native `TabInterface` & Storage Bridges)**:
  - `TabAndroid` implementing cross-platform `tabs::TabInterface` and owning `content::WebContents` and `tabs::TabFeatures`, alongside Android-specific JNI packager and service adapters for `TabStateStorageService`.

---

## 2. Tab Terminology & State Definitions

- **Current or Active (`Tab.isActivated()`, `TabModel.getCurrentTabSupplier()`, `TabList.index()`)**:
  - The single tab currently activated in its `TabModel`. (Note: an inactive `TabModel` in `TabModelSelector` still maintains its own current/active tab).
- **Selected (`Tab.isMultiSelected()`, `TabModel.isTabMultiSelected()`, `TabInterface::IsSelected()`)**:
  - Present in the multi-selection set. Distinct from the current/active tab, though `Tab.isMultiSelected()`, `TabModel.isTabMultiSelected()`, and `TabInterface::IsSelected()` all implicitly include the current/active tab when attached to a `TabModel` (returning `false` when detached). Note that `TabCollectionTabModelImpl#getMultiSelectedTabsCount()` returns `0` whenever `getCurrentTabSupplier().get() == null` (including non-empty models with no active tab yet, such as during background restore before `completeInitialization()`), and in `ARCHIVED` models `index()` returns `INVALID_TAB_INDEX` (so `isTabMultiSelected()` returns `false` unless explicitly selected).
- **Foreground vs. Background vs. Shown / Hidden (`TabLifecycle.show()`, `hide()`, `isHidden()`, `TabInterface::IsVisible()`)**:
  - **Foreground**: Foreground in its window. Historically synonymous with Current/Active, but with split tabs (`crbug.com/480192698`) multiple tabs in a split are foreground simultaneously while only one is Current/Active.
  - **Background**: Not foregrounded in its window.
  - **Shown vs. Hidden (`!isHidden()` vs. `isHidden()`)**: Tracks whether the tab's view is shown. Distinct from window-level foreground/background because a foreground tab is also hidden (`isHidden() == true`) when the Activity is backgrounded (`TabHidingType.ACTIVITY_HIDDEN`) or when the Tab Switcher / Hub is shown (`TabHidingType.TAB_SWITCHER_SHOWN`). (Note: `TabHidingType.REPARENTED` is unused; reparenting toggles `Tab.isDetachedFromActivity()` rather than `isHidden()`).
  - **Window Occluded (`WindowAndroid.getOcclusionSupplier()`) vs. Obscured (`TabObscuringHandler`) vs. User Interactable (`Tab.isUserInteractable()`)**:
    - **Window Occluded**: OS-level window occlusion (`WindowAndroid.getOcclusionSupplier()`), which transitions `WebContents` to `Visibility.OCCLUDED`.
    - **Obscured**: In-app UI covering tabs/toolbar (`TabObscuringHandler`), which hides views from accessibility (`IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS`), clears accessibility focus (`WebContentsAccessibilityImpl#setObscuredByAnotherView`), and blocks descendant Android view focus traversal on `CompositorViewHolder` (`FOCUS_BLOCK_DESCENDANTS`) while keeping `WebContents` `Visibility.VISIBLE`.
    - **User Interactable (`Tab.isUserInteractable()`)**: True iff `!mIsHidden && !isFrozen() && mIsViewAttachedToWindow && !isDetachedFromActivity()`.
- **Frozen (`Tab.isFrozen()`) vs. `FrozenNativePage` (`NativePage.isFrozen()`) vs. Discarded (`Tab.discard()`)**:
  - **`Tab.isFrozen()` (`!isNativePage() && getWebContents() == null`)**: The tab has neither a `WebContents` nor a `NativePage`.
  - **`FrozenNativePage` (`NativePage.isFrozen()` / `Tab.freezeNativePage()`)**: Separate mechanism replacing a backgrounded `NativePage` with a viewless stand-in; `Tab.isFrozen()` remains `false`.
  - **Discarded (`Tab.discard()`)**: Discards `WebContents` in-place (`mWebContents != null`, `Tab.isFrozen() == false`, `needsReload() == true`).
- **Window/Activity Detachment vs. Model/Collection Detachment**:
  - **Window/Activity Detachment (`Tab.isDetachedFromActivity()`, `Tab.updateAttachment()`)**: Either `WindowAndroid` is null or has no `Activity` (`HiddenTabHolder`, spare tabs, `HEADLESS` model), or the tab was detached via `updateAttachment(null, null)` during reparenting (which clears `webContents.setTopLevelNativeWindow(null)` and sets `mIsDetachedFromActivity = true` while retaining a non-null `WindowAndroid` until re-attachment; see Section 3.E).
  - **Model/Collection Detachment (`Tab.onRemovedFromTabModel()`, `Tab.hasParentCollection()`, `TabInterface::RegisterWillDetach`)**: Removed from its `TabModel` / `TabStripCollection`.
- **Dormant Models (`ARCHIVED` & `HEADLESS`)**:
  - `TabModel.isDormantTabModel()` models never hold `WebContents`; `HEADLESS` is intended to become strictly read-only in the future.

---

## 3. Key Architectural Invariants

### A. The 1:1 `Tab`-to-`WebContents` Invariant (Post-Unfreeze) & In-Place Discarding
- Once a `Tab` transitions from frozen (`mWebContents == null`) to initialized (`TabImpl#initWebContents` / `TabAndroid::InitWebContents`), it maintains a **strict, permanent 1:1 relationship** with that exact `content::WebContents` instance until the `Tab` is destroyed (`TabImpl#destroyInternal`).
- `swapWebContents` no longer exists on `TabImpl`, and `TabAndroid::RegisterWillDiscardContents` is a no-op because discarding on Android **never replaces** the `WebContents` object.
- Instead, `TabImpl#discard()` invokes `mWebContents.discard()` (`content::WebContents::Discard()`), which tears down the underlying frame tree and renderer process **in-place** while keeping the `WebContents` wrapper instance (`mWebContents != null`), its `tabs::TabFeatures` instance, and `TabLookupFromWebContents` bindings intact.
- Even when C++ takes ownership of a `WebContents` (`TabAndroid::TakeWebContentsAndDestroyTab`), it synchronously destroys the `TabImpl` rather than leaving a live `Tab` with a null or replaced `WebContents`.

### B. How Desktop Android Uses `LoadAllTabsAtStartup` to Satisfy C++ `TabInterface` Contracts
- Shared desktop C++ subsystems (`BrowserWindowInterface`, `TabListInterface`, `tabs::TabFeatures`, Chrome Extensions `chrome.tabs`, DevTools, and `PerformanceManager`) require `TabInterface::GetContents()` to **always** return a non-null `content::WebContents*`. On mobile Android, restored and lazy background tabs remain frozen (`mWebContents == null`), making them invisible to C++ consumers that require a `WebContents*`.
- Desktop Android bridges this gap by enabling `kLoadAllTabsAtStartup` (`ChromeFeatureList.sLoadAllTabsAtStartup`, paired with `kWebContentsDiscard`, `kLazyBrowserInterfaceBroker`, and `kDesktopAndroidBackgroundTabLoading`).
- When `sLoadAllTabsAtStartup` is enabled (`mIsContentViewDeferred == true` on non-dormant `TabModel`s):
  1. **Rendererless Startup Unfreeze**: `TabImpl#initialize` immediately unfreezes restored tabs via `unfreezeContents(/* noRenderer= */ true)` (restoring the `NavigationController` from `WebContentsState` **without** allocating a renderer process, leaving `needsReload() == true`) or initializes a rendererless `WebContents` for lazy-load tabs while preserving `mPendingLoadParams`.
  2. **Deferred View Inflation (`DeferredContentViewStub`)**: To prevent severe startup UI jank from inflating `ContentView` view hierarchies for hundreds of tabs at startup, `TabImpl#initWebContents` installs a lightweight `DeferredContentViewStub` instead of a real `ContentView`. The real `ContentView` is inflated on demand via `maybeInflateContentView()` when the tab is actually shown (`TabImpl#show` / `restoreIfNeeded`).
  3. **Model-Level Assertions**: `TabCollectionTabModelImpl#maybeAssertTabHasWebContents` enforces `tab.getWebContents() != null` on `STANDARD` tab models when `sLoadAllTabsAtStartup` is enabled, and conversely asserts `tab.getWebContents() == null` on dormant (`ARCHIVED` / `HEADLESS`) tab models.
  4. **Background Tab Loading Policy**: Once session restore finishes, `TabModelJniBridge::BroadcastSessionRestoreComplete` triggers `performance_manager::policies::ScheduleLoadForRestoredTabs` to progressively load background tabs under memory and CPU constraints.

### C. Tab Lifecycle States & `TabCreationState` Nuances
A `Tab` exists in one of the following mutually distinct runtime states:

1. **Frozen (`isFrozen()`)**:
   - Defined as `!isNativePage() && getWebContents() == null`.
   - Backed by serialized `mWebContentsState` (`ByteBuffer`) or `mPendingLoadParams` (`LoadUrlParams`).
   - *Nuance*: If `isNativePage()` is `true`, `isFrozen()` returns `false` even when `getWebContents() == null` (note that backgrounded `NativePage`s can themselves be swapped to a viewless `FrozenNativePage` proxy).
2. **Loaded with Live Renderer**:
   - `mWebContents != null && !needsReload() && !SadTab.isShowing(tab)`.
3. **Loaded with No Renderer (`mWebContents != null`, `isFrozen() == false`)**:
   - Occurs in four distinct scenarios:
     - **Startup rendererless unfreeze**: `unfreezeContents(/* noRenderer= */ true)` leaves `needsReload() == true`.
     - **In-place discard**: `mWebContents.discard()` tears down the renderer and sets `needsReload() == true`.
     - **Background renderer crash / OS OOM kill**: When `TabWebContentsObserver#primaryMainFrameRenderProcessGone` fires while the tab is hidden or the Activity is stopped, it calls `mTab.setNeedsReload()` **without** showing `SadTab`. When the user selects the tab later (`show()` -> `loadIfNeeded()` -> `restoreIfNeeded()`), the tab transparently reloads.
     - **Foreground renderer crash**: When `primaryMainFrameRenderProcessGone` fires while the tab is visible, it attaches the `SadTab` overlay via `TabViewManager` **without** setting `needsReload()`, displaying the "Aw, Snap!" UI.
     - NOTE: `needsReload()` is not itself an indicator the renderer is absent, it is necessary to check if `getWebContents().getRenderWidgetHostView() == null` to check this if there is no renderer.
4. **Closing (`isClosing()`) vs. Destroyed (`isDestroyed()`)**:
   - `mIsClosing` is **reversible**: set while a tab is pending undoable closure in the snackbar queue (`mWebContents` and `TabAndroid` remain alive).
   - `mIsDestroyed` is **terminal**: `destroyInternal()` tears down `mWebContents`, native `TabAndroid`, `TabHelpers`, and removes the tab from `sTabMap`.

#### `TabCreationState` & `TabStateAttributes` Dirtiness
`TabCreationState` (`LIVE_IN_FOREGROUND`, `LIVE_IN_BACKGROUND`, `FROZEN_ON_RESTORE`, `FROZEN_FOR_LAZY_LOAD`) directly determines initial persistence dirtiness in `TabStateAttributes`:
- `FROZEN_FOR_LAZY_LOAD`: Immediately starts at `DirtinessState.DIRTY` to force an initial disk write, because a lazy background tab will not fire any `WebContents` navigation completion events while unvisited.
- `LIVE_IN_FOREGROUND` / `LIVE_IN_BACKGROUND`: Starts at `DirtinessState.UNTIDY`.
- `FROZEN_ON_RESTORE`: Starts at `DirtinessState.CLEAN` (already persisted on disk).

### D. Pending Navigations (`mPendingLoadParams`, `discardAndAppendPendingNavigation`, `loadIfNeeded`)
- **URL & Title Protection**: When `mPendingLoadParams != null`, `TabImpl#getUrl()` returns the pending `GURL` rather than querying `mWebContents` or `mWebContentsState`, and `TabImpl#updateTitle()` early-outs so an unnavigated rendererless `WebContents` (created under `LoadAllTabsAtStartup`) cannot clobber the tab's title with an empty string.
- **Mutual Exclusion Invariant in `discardAndAppendPendingNavigation`**:
  A `Tab` **never** holds both `mWebContentsState` and `mPendingLoadParams` simultaneously. Calling `TabImpl#discardAndAppendPendingNavigation(params, title)` handles three cases:
  1. **Lazy Tab (`mWebContents == null && mWebContentsState == null`)**: Replaces `mPendingLoadParams` in-place.
  2. **Frozen Tab (`mWebContentsState != null`)**: Serializes the pending navigation directly into the pickled `WebContentsState` buffer via `WebContentsState.appendPendingNavigation(...)`. In C++, `WebContentsState::AppendPendingNavigation` falls back to creating a single-entry `WebContentsStateByteBuffer` if unpickling or the off-the-record profile check fails; only if JNI `ByteBuffer` creation itself returns `null` does Java clear `mWebContentsState` and fall back to setting `mPendingLoadParams`.
  3. **Live/Discarded Tab (`mWebContents != null`)**: Discards `mWebContents` in-place (`DiscardReason.APPEND_NAVIGATION`) and stores `mPendingLoadParams`.
- **Consumption in `loadIfNeeded()`**:
  - If `mPendingLoadParams != null`, `loadIfNeeded()` initializes `WebContents` (if still null), calls `loadUrl(mPendingLoadParams)`, and clears `mPendingLoadParams = null`.
  - Otherwise, it delegates to `restoreIfNeeded()`, which calls `unfreezeContents(/* noRenderer= */ false)` and `navigationController.loadIfNecessary()`.

### E. Tab Attachment & Detachment (`updateAttachment`, Reparenting, `WindowAndroid`, `TabDelegateFactory`)
`TabImpl#updateAttachment(@Nullable WindowAndroid window, @Nullable TabDelegateFactory tabDelegateFactory)` operates in three distinct modes:

1. **Detachment (`window == null && tabDelegateFactory == null`)**:
   - **Non-Obvious `mWindowAndroid` Quirk**: `TabImpl.mWindowAndroid` is **NOT** set to `null` (preserving a valid reference so callers of `getWindowAndroidChecked()` do not crash with `NullPointerException`). Instead, `ReparentingTask.detach()` sets `webContents.setTopLevelNativeWindow(null)` and `TabImpl` sets `mIsDetachedFromActivity = true`. Calling `updateAttachment(null, null)` without clearing `webContents.setTopLevelNativeWindow(null)` will trip the assertion inside `isDetachedFromActivity()`.
   - Fires `TabAndroid::SendWillDetachUpdate` (notifying `TabInterface::RegisterWillDetach` subscribers), clears `mCurrentTabSupplier`, and notifies `TabObserver.onActivityAttachmentChanged(tab, /* window= */ null)`.
2. **Full Re-attachment (`window != null && tabDelegateFactory != null`)**:
   - **Strict Ordering Requirement**: `setDelegateFactory(tabDelegateFactory)` **MUST** execute before `updateWindowAndroid(window)` because `updateWindowAndroid` queries fullscreen state on the new `WebContentsDelegate`.
   - Recreates `TabWebContentsDelegateAndroid`, `ContextMenuPopulatorFactory`, and `BrowserControlsVisibilityDelegate`, calls `TabAndroid::UpdateDelegates`, rebinds `webContents.setTopLevelNativeWindow(window)`, and **freezes or force-reloads any live `NativePage`** (because `NativePage` views hold strong references to the old `Activity`, whereas `mContentView` is constructed with `getThemedApplicationContext()` and survives reparenting across Activities without recreation).
   - `ReparentingTaskJni.attachTab` flushes any `BackgroundTabManager` history accumulated while detached.
3. **Window-Only Swap (`window != null && tabDelegateFactory == null`)**:
   - Updates `WindowAndroid` without replacing `TabDelegateFactory` and **does NOT fire `TabObserver.onActivityAttachmentChanged`**.
- **Prewarmed / Hidden Tabs (`HiddenTabHolder`)**: Have a non-null `WindowAndroid` backed by an Application `Context` (`windowHasActivity(window) == false`), so `isDetachedFromActivity()` returns `true` until attached to an Activity.

### F. Additional Non-Obvious Architectural Details
- **Java Owns Native `TabAndroid` & Static `sTabMap` Scaling**:
  - `TabImpl` owns the native `TabAndroid` lifecycle (`TabAndroid::DeleteSelf()` is a no-op; `~TabAndroid()` `CHECK`s `!parent_collection_`, requiring removal from its `TabCollection` before `TabImpl.destroy()`).
  - To prevent exhausting the Android JVM's finite JNI `GlobalRef` table when users accumulate thousands of tabs, `TabAndroid` does **not** hold a `ScopedJavaGlobalRef` to `TabImpl`. Instead, `TabImpl` maintains a static `LongSparseArray<TabImpl> sTabMap` mapping `mNativeTabAndroid -> TabImpl`, and `TabAndroid` holds no JNI reference field (`GlobalRef` or `WeakGlobalRef`) at all—resolving its Java peer on demand by passing `reinterpret_cast<intptr_t>(this)` into `TabImpl.sTabMap`. Any other per-tab or per-`WebContents` JNI references should follow this pattern.
- **Bidirectional `TabCollection` Property Sync**:
  - Moving a `TabAndroid` within a C++ `TabStripCollection` hierarchy triggers `TabAndroid::OnAncestorChanged` -> `UpdateProperties()`, which pushes derived `isPinned` and `tabGroupId` properties down to Java `TabImpl`, notifying `TabObserver`s and invoking C++ `TabInterface` callbacks.
  - When a tab is detached from its collection (`parent_collection_ == nullptr`), `UpdateProperties()` is intentionally skipped so `TabImpl` retains its last known `isPinned` and `tabGroupId` state across reparenting and undoable closures.
- **View Hierarchy Precedence (`TabImpl#getView()`) & `isUserInteractable()`**:
  - `TabImpl#getView()` resolves in strict precedence order:
    1. `mCustomView` (managed by `TabViewManagerImpl` with priority `SUSPENDED_TAB` > `PAINT_PREVIEW` > `SAD_TAB`).
    2. `mNativePage.getView()` (if showing a `NativePage` and not frozen).
    3. `mContentView` (`ContentView` or `DeferredContentViewStub`).
  - Because `mAttachStateChangeListener` is registered **only** on `mContentView` and `mNativePage.getView()`, presenting a `mCustomView` detaches `mContentView` from its parent `ViewGroup` and causes `TabImpl#isUserInteractable()` to become `false`.
- **Per-Tab Data Attachment Mechanisms**:
  - `UserDataHost` (`Tab.getUserDataHost()`): Java objects scoped to the `Tab` lifetime (survives unfreezing and discarding).
  - `TabWebContentsUserData`: Java helpers bound to the `Tab`'s `WebContents` lifecycle (initialized in `TabHelpers.initWebContentsHelpers`).
  - `tabs::TabFeatures` & `UnownedUserDataHost`: C++ cross-platform features scoped to `TabAndroid` / `TabInterface`.
  - `PersistedTabData`: Asynchronously persisted per-tab key-value data (`ShoppingPersistedTabData`, `ArchivePersistedTabData`).
  - `TabStateAttributesRegistry`: Tracks tab persistence dirtiness (`DirtinessState`) with **separate tracking per `StoreKey`** so legacy `TabPersistentStoreImpl` and modern `TabStateStore` can run in shadow/migration mode without clearing each other's dirty bits.
- **`WebContentsState` GC-Managed Direct Buffers**:
  - `WebContentsState` is immutable wrt its GC-managed direct `ByteBuffer` (`ByteBuffer.allocateDirect`, allocated on the background DB sequence when loading `StorageLoadedData` on Android) and has no `destroy()` method; `appendPendingNavigation` and `deleteNavigationEntries` return new `WebContentsState` instances rather than mutating the buffer in place.
- **Deprecated APIs (Do Not Use in New Code)**:
  - `getRootId()` / `setRootId()`: Deprecated; use `getTabGroupId()` (`Token`) and `TabCollection` hierarchy APIs instead.
  - `getMediaState()` / `setMediaState()`: Deprecated; use `getAlertState()` / `tabs::TabAlertController` instead.
  - `isIncognito()`: Deprecated on `Tab`; use `isOffTheRecord()` (all OTR profiles) or `isIncognitoBranded()` (specifically user-facing Incognito) instead.
