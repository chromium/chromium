New Tab Page (Desktop)
======================

On Desktop (ChromeOS, Windows, Mac, and Linux), there are multiple
variants of the **New Tab Page** (**NTP**). The variant is selected
according to the user’s **Default Search Engine** (**DSE**). All
variants are implemented as HTML/CSS/JS, but differ in where they are
hosted.

*   **Google**: The **[Local NTP][local-ntp]**, with Google branding.

*   **Bing**, **Yandex**: a **[Third-Party NTP][third-party-ntp]**,
    where the NTP is hosted on third-party servers but Chrome provides
    some of the content via <iframe> elements.

*   **Other**: the **Local NTP** with no branding.

As of 2017-12-05, Bing and Yandex have implemented third-party NTPs. The
full list is in [`prepopulated_engines.json`][engines], under the key
`"new_tab_url"`.

Non-Google variants show up to 8 **NTP Tiles**. Each NTP tile represents a site
that Chrome believes the user is likely to want to visit. On Desktop, NTP tiles
have a title, a large icon, and an “X” button so that the user can remove tiles
that they don’t want.

Google variants show up to 10 **NTP Tiles** (now called shortcuts) and give
users the ability to customize them. This includes adding new shortcuts using
the "Add shortcut" button, deleting/editing shortcuts from the three-dot "Edit
shortcut" menu (replaces the "X" button), and reordering via click-and-drag.

[local-ntp]:        #local-ntp
[third-party-ntp]:  #third_party-ntp
[engines]:          https://chromium.googlesource.com/chromium/src/+/master/components/search_engines/prepopulated_engines.json

Variants
--------

### Local NTP

#### Google branding

##### One Google Bar

The **One Google Bar** (**OGB**) is at the top of the NTP. The NTP
fetches the OGB from Google servers each time it loads.

##### Google Logo

The **Google Logo** is below the OGB. By default, it is the regular
Google logo. It can also be a **Doodle**, if a Google Doodle is running
for a particular combination of (today’s date, user’s country, user’s
birthday).

###### No Doodle

On a day when there is no Doodle (in the user’s current country), the
NTP shows the Google logo. It comes in two variants:

*   Colorful, if the user is using the default theme, or on any other
    theme with a solid black (L=0%), white (L=100%), or gray (S=0%)
    background color.
*   White, if the user’s theme has a background image, or if the
    background is a solid color, but not black, white, or gray.

Also, even on days when there is a Doodle, if the user’s theme’s
background is not solid white, new NTPs show the Google logo by default.
In this case, an animated spinner advertises the Doodle. If the user
clicks on the spinner, then the NTP resets to the default theme and
shows the Doodle.

###### Static Doodles

A **Static Doodle** shows as a single static image. When clicked, it
triggers a navigation to the Doodle’s target URL.

###### Animated Doodles

An **Animated Doodle** initially shows a static **Call-to-Action**
(**CTA**) image, usually with a “play” icon. When clicked, it swaps out
the CTA image for an animated image. When clicked a second time, it
triggers a navigation to the Doodle’s target URL.

###### Interactive Doodles

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

##### Fakebox

The **Fakebox** looks like a search bar, so that the NTP mimics the
appearance of the Google homepage. It’s not actually a real search bar,
and if the user interacts with it, the NTP moves keyboard focus and any
text to the Omnibox and hides the Fakebox.

##### Search Suggestions

Above the NTP tiles there is space for search suggestions. Search suggestions
are typically 3-4 queries recommended to logged-in users based on their previous
search history.

Search suggestions are fetched from Google servers on NTP load and cached to be
displayed on the following NTP load.

##### Middle-slot Promos

Below the NTP tiles, there is space for a **Middle-slot Promo**. A promo is
typically a short string, typically used for disasters (e.g. “Affected
by the Boston Molassacre? Find a relief center near you.”) or an
advertisement (e.g. “Try the all-new new Chromebook, with included
toaster oven.”).

Middle-slot promos are fetched from Google servers on NTP load.

#### Non-Google Local NTP

A non-Google local NTP shows only NTP tiles, with no branding. The tiles
are centered within the page.

### Third-Party NTP

TODO(sfiera)
