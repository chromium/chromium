WebUI New Tab Page (Desktop)
============================

On Desktop (ChromeOS, Windows, Mac, and Linux), there are multiple variants of
the **New Tab Page** (**NTP**). The variant is selected according to the user’s
**Default Search Engine** (**DSE**), profile, extensions and policies. This
folders implements the backend of the first-party Google NTP. The features this
variant supports are detailed in the following sections.

# Features

## One Google Bar

The **One Google Bar** (**OGB**) is at the top of the NTP. The NTP
fetches the OGB from Google servers each time it loads.

## Google Logo

On a day when there is no Doodle (in the user’s current country), the
NTP shows the **Google Logo**. It comes in two variants:

*   Colorful, if the user is using the default theme, or on any other
    theme with a solid black (L=0%), white (L=100%), or gray (S=0%)
    background color.
*   White, if the user’s theme has a background image, or if the
    background is a solid color, but not black, white, or gray.

## Doodle

The **Doodle** replaces the Google Logo on days a doodle is available. The
doodle comes in three flavors:

### Static Doodles

A **Static Doodle** shows as a single static image. When clicked, it
triggers a navigation to the Doodle’s target URL.

### Animated Doodles

An **Animated Doodle** initially shows a static **Call-to-Action**
(**CTA**) image, usually with a “play” icon. When clicked, it swaps out
the CTA image for an animated image. When clicked a second time, it
triggers a navigation to the Doodle’s target URL.

### Interactive Doodles

An **Interactive Doodle** is embedded into the NTP as an `<iframe>`.
The framed content usually contains a CTA image, but this is opaque to
the containing NTP.

The embedded Doodle can ask the containing NTP to resize the `<iframe>`
tag to enlarge the space available for the Doodle. To do this, it sends
a `postMessage()` call to `window.parent`. The event data supports these
parameters:

*   `cmd` (required string): must be `"resizeDoodle"`.
*   `width` (required string): a CSS width (with units). Because the
    Doodle cannot know the size of the outer page, values based on
    `"100%"` (e.g. `"100%"` or `"calc(100% - 50px)"`) are recommended.
*   `height` (required string): a CSS height (with units). Must not be a
    percentage, but otherwise any units are OK.
*   `duration` (optional string): a CSS duration, such as `"130ms"` or
    `"1s"`. If `null` or absent, `"0s"` (no transition) is assumed.

For example:

    // Reset to initial width and height.
    window.parent.postMessage({cmd: "resizeDoodle"});

    // Transition smoothly to full-width, 350px tall.
    window.parent.postMessage({
        cmd: "resizeDoodle",
        width: "100%",
        height: "350px",
        duration: "1s",
    });

### Realbox

The **Realbox** is a search bar to make Google queries similar to the Omnibox.

## Most Visited Tiles

The NTP shows up to 10 **NTP Tiles** (now called shortcuts) and give
users the ability to customize them. This includes adding new shortcuts using
the "Add shortcut" button, deleting/editing shortcuts from the three-dot "Edit
shortcut" menu (replaces the "X" button), and reordering via click-and-drag.

### Middle-slot Promos

Below the NTP tiles, there is space for a **Middle-slot Promo**. A promo is
typically a short string, typically used for disasters (e.g. “Affected
by the Boston Molassacre? Find a relief center near you.”) or an
advertisement (e.g. “Try the all-new new Chromebook, with included
toaster oven.”).

Middle-slot promos are fetched from Google servers on NTP load.
