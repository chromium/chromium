# Legacy Browser Support (BrowserSwitcher internally)

BrowserSwitcher is a Chrome module that listens to navigations, and
automatically switches to another browser (typically IE) for a predefined set of
URLs.

It is a port of the old Legacy Browser Support extension, to make it easier to
deploy across organizations.

Setup instructions for administrators can be found
[here](https://support.google.com/chrome/a/answer/9270076).

## Configuration

The policies in the [BrowserSwitcher
group](https://www.chromium.org/administrators/policy-list-3#BrowserSwitcher)
let admins configure this feature, to decide which URLs should open in Chrome
and which should open in the alternate browser.

### Sitelist and Greylist

There are 2 types of rules for LBS:

* Sitelist (AKA URL list): when the user visits one of these URLs in Chrome, it
  opens in the alternate browser. If any other URL is viewed in IE, it bounces
  back to Chrome.

* Greylist: these URLs do not trigger a browser switch. i.e., it stays in Chrome
  if viewed in Chrome, and it stays in IE if viewed in IE.

These rules can be applied from 3 different sources:

* Directly, with Chrome policies:
  [BrowserSwitcherUrlList](https://www.chromium.org/administrators/policy-list-3#BrowserSwitcherUrlList)
  and
  [BrowserSwitcherUrlGreylist](https://www.chromium.org/administrators/policy-list-3#BrowserSwitcherUrlGreylist)
  control the sitelist and the greylist, respectively.

* EMIE site list: IE/Edge can be
  [configured](https://docs.microsoft.com/en-us/internet-explorer/ie11-deploy-guide/turn-on-enterprise-mode-and-use-a-site-list)
  to open websites in IE, with a certain renderer version. BrowserSwitcher can
  share the same rules IE uses, using the
  [BrowserSwitcherUseIeSitelist](https://www.chromium.org/administrators/policy-list-3#BrowserSwitcherUseIeSitelist)
  policy. The rules are specified as a URL, that points to an XML file that
  Chrome downloads.

* Other XML site list: Specifies a URL to an XML file (like the EMIE site list),
  but the rules aren't shared with IE. These rules are controlled by the
  [BrowserSwitcherExternalSitelistUrl](https://www.chromium.org/administrators/policy-list-3#BrowserSwitcherExternalSitelistUrl)
  and
  [BrowserSwitcherExternalGreylistUrl](https://www.chromium.org/administrators/policy-list-3#BrowserSwitcherExternalGreylistUrl)
  policies.

If rules from multiple sources are present, they are combined into one
list. This means you can create some rules with Chrome policies, and add more
rules from the EMIE site list.

If multiple rules match one navigation, then the longest rule applies. For
instance:

1. Let's say `sitelist = [ "example.com", "!foo.example.com" ]`
2. User visits `http://foo.example.com/` in Chrome
3. The website opens in Chrome, because `!foo.example.com` is longer than
  `example.com`, and it starts with a `!` (which inverts the rule).

### Debugging/Troubleshooting

Enterprise admins and developers can visit the
`chrome://browser-switch/internals` page to view the state of LBS. This page
displays the list of rules, and lets you re-download XML sitelists immediately.

## BHO (unsupported)

On Windows, a BHO (an IE add-on) can be used to automatically bounce back to
Chrome from IE when visiting a non-whitelisted URL.

### Sharing State with Chrome

The BHO cannot access all Chrome policies, which are needed to decide if a
navigation should bounce back to Chrome.

To solve this problem, BrowserSwitcher writes a `cache.dat` file in
`AppData\Local\Google\BrowserSwitcher`.  It contains the sitelist + greylist in
a format that's easy to parse for the BHO. Whenever new rules are added or
removed, it re-writes the `cache.dat` file.

This is the same mechanism that the old extension uses, so this feature is
compatible with the old BHO.

## Edge extension

When Edge switched to a Chromium-based fork, they added an IE integration mode.
This is how Microsoft recommends running legacy applications, and standalone IE
is [unsupported since June
2022](https://learn.microsoft.com/en-us/lifecycle/faq/internet-explorer-microsoft-edge).
For instance, Windows 11's version of IExplore.exe cannot be used as an actual
browser.

We offer an Edge extension, which is functionally and architecturally similar to
the old IE BHO.

You can find the extension here:
https://microsoftedge.microsoft.com/addons/detail/legacy-browser-support-fo/acallcpknnnjahhhapgkajgnkfencieh
