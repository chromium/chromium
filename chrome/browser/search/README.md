Instant New Tab Page (Desktop)
==============================

On Desktop (ChromeOS, Windows, Mac, and Linux), there are multiple
variants of the **New Tab Page** (**NTP**). The variant is selected
according to the user’s **Default Search Engine** (**DSE**), profile, extensions
and policies. This folder implements the backend of third-party instant NTPs for
search engines such as **Bing** and **Yandex**. The full list is in [`prepopulated_engines.json`][engines], under the key `"new_tab_url"`.

Instant NTPs are loaded at runtime from third-party servers and gain special privileges over regular web pages. For example, instant NTPs can embed up to 8
**NTP Tiles**. vie <iframe> elements. Each NTP tile represents a site that
Chrome believes the user is likely to want to visit. On Desktop, NTP tiles have
a title, a large icon, and an “X” button so that the user can remove tiles that
they don’t want.
