# AppsFetcherService

Chrome OS has apps that can come from a wide variety of app platforms or app
providers. E.g.
- PWAs (Progressive Web Apps)
- ARC++ (Android apps)
- Crostini (Linux apps)
- Borealis

The AppsFetcherService acts as an intermediary between apps consumers and apps
providers. This intermediary is useful because there is not a 1:1 but rather a
1:n relationship between apps consumers and apps providers: for a given apps
consumer, we might need to fetch apps from different providers. This is
especially true for user interfaces; for instance, when the search bar has to
surface games following a user request, the apps list returned by the service
can contain games from a variety of apps platforms.

---

Updated on 2021-07-20.
