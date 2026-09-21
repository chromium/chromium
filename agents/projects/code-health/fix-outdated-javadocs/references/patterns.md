# Implementation Patterns: Fix Outdated Javadocs

Reference guide for Javadoc cleanups per the
[Google Java Style Guide (§ 7)](https://google.github.io/styleguide/javaguide.html#s7-javadoc).

______________________________________________________________________

## 1. Missing Parameter Tag

When a method signature has parameters omitted from an existing Javadoc comment:

```java
// Bad:
/**
 * Resets the zoom factor.
 * @param webContents The web contents to reset.
 */
public static void zoomReset(WebContents webContents, BrowserContextHandle handle)

// Good:
/**
 * Resets the zoom factor.
 * @param webContents The web contents to reset.
 * @param handle The browser context handle to get the default zoom level from.
 */
public static void zoomReset(WebContents webContents, BrowserContextHandle handle)
```

______________________________________________________________________

## 2. Stale or Renamed Parameter Tag

When a parameter was renamed, has a casing typo, or was removed from the
signature:

```java
// Bad:
/**
 * @param backPressmanager Handler for back presses.
 * @param legacyId Removed parameter no longer in signature.
 */
public DownloadPage(BackPressManager backPressManager)

// Good:
/**
 * @param backPressManager Handler for back presses.
 */
public DownloadPage(BackPressManager backPressManager)
```

______________________________________________________________________

## 3. Return-Only Comments (`"Returns ..."`)

`@return` without a summary fragment violates § 7.2. Convert to a third-person
summary fragment `/** Returns ... */`:

```java
// Bad:
/** @return the theme ID to use. */
public static int getThemeId()

// Good:
/** Returns the theme ID to use. */
public static int getThemeId()
```

*(Multi-line blocks containing only `@return` should likewise be converted to a
summary fragment, compressing to a single line if under 100 characters).*

______________________________________________________________________

## 4. Inline Code References (`|param|` or `` `param` `` $\\rightarrow$ `{@code param}`)

In Java comments, replace C++ pipe delimiters (`|param|`) and Markdown backticks
(`` `param` ``) with standard `{@code param}` (§ 7.3.1):

```java
// Bad:
/** Matches the given filter against |url| and `options`. */
boolean matches(String url, Options options);

// Good:
/** Matches the given filter against {@code url} and {@code options}. */
boolean matches(String url, Options options);
```

______________________________________________________________________

## 5. Invalid Inline `{@param}` Tag

`@param` is strictly a block tag. Replace invalid inline `{@param name}` usages
with `{@code name}`:

```java
// Bad:
/** Applies configuration for {@param settings}. */

// Good:
/** Applies configuration for {@code settings}. */
```

______________________________________________________________________

## 6. Block Tag Order & Indentation

Standard block tags must appear in canonical order: `@param`, `@return`,
`@throws`, `@deprecated`. Continuation lines are indented 4 spaces:

```java
// Bad:
/**
 * @return The fetched data.
 * @param url The target URL. If invalid, the request
 * will fail.
 */

// Good:
/**
 * @param url The target URL. If invalid, the request
 *     will fail.
 * @return The fetched data.
 */
```

______________________________________________________________________

## Core Rules

1. **No Code Changes:** Do NOT touch method signatures, parameter names, or
   method bodies.
2. **No Empty Tags:** `@param` and `@return` must never have an empty
   description.
3. **Preserve Content:** Keep existing summary text, HTML tags, and `@link`
   annotations intact.
4. **No Missed Parameters:** If a method's Javadoc documents parameters, ensure
   ALL parameters in the method signature have a corresponding `@param` tag.
5. **Existing Javadocs Only:** Do not add brand-new Javadoc blocks to
   undocumented or private methods.
