---
name: general/use-standard-technical-terminology-in-comments
rubric: readability
severity: should_fix
---

**Rule.** In code comments and technical documentation, use official platform
names and specification terminology (such as 'SDK API level', 'the Android
platform', or 'the host OS'), standard technical terms (such as 'housekeeping',
'bookkeeping', or container 'erased'/'removed'), exact identifier names and
capitalization (such as `ByteSize` functions), and direct descriptions of
concrete mechanisms. Do not use informal colloquialisms (such as 'paperwork'),
ambiguous references like 'the platform', obscure design-pattern jargon (such as
'LazyHolder'), negative phrasing ('Not a ...'), vague lifecycle descriptions
('sets up'), or inaccurate domain concepts (such as conflating endianness with
bitness, intermediate with leaf frames, or container mutations as 'physically
erased').

**Why.** Obscure design-pattern jargon, negative definitions, colloquialisms,
and imprecise or ambiguous terms (such as referring generically to 'the
platform' in a multi-layered codebase) confuse readers who may not share niche
background knowledge or obscure official platform contracts. Direct
plain-language descriptions, exact symbol names, and official platform and
target environment terminology make comments self-contained, searchable, and
universally understandable across diverse contributors.

## Examples

Bad:

```cpp
// Returns the first Android level the device had.
static std::string GetAndroidFirstApiLevel();

// Callbacks canceled during iteration are physically erased.
void ErasePendingCallbacks();

// Note: We use UmaHistogramCounts10M() instead of one of the byte size
// functions because we care about values under 1MB.

// Not a LazyHolder to facilitate testing.
private static @Nullable ScreenOffBroadcastReceiver sInstance;

/**
 * Sends an intent to the platform to display a dialog about the restricted content.
 */
default boolean sendShowRestrictedContentIntent(Uri uri) {
    return false;
}
```

Good:

```cpp
// Returns the SDK API level that the device is initially launched with.
static std::string GetAndroidFirstApiLevel();

// Callbacks canceled during iteration are erased.
void ErasePendingCallbacks();

// Note: We use UmaHistogramCounts10M() instead of one of the ByteSize
// functions because we care about values under 1MB.

// Initialize lazily to facilitate testing.
private static @Nullable ScreenOffBroadcastReceiver sInstance;

/**
 * Sends an intent to the Android platform to display a dialog about the restricted content.
 */
default boolean sendShowRestrictedContentIntent(Uri uri) {
    return false;
}
```
