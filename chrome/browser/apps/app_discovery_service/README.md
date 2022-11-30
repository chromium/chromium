# AppDiscoveryService

Chrome OS has apps that can come from a wide variety of app platforms or app
providers. E.g.
- PWAs (Progressive Web Apps)
- ARC++ (Android apps)
- Crostini (Linux apps)
- Borealis

The AppDiscoveryService acts as an intermediary between apps consumers and apps
providers. This intermediary is useful because there is not a 1:1 but rather a
1:n relationship between apps consumers and apps providers: for a given apps
consumer, we might need to fetch apps from different providers. This is
especially true for user interfaces; for instance, when the search bar has to
surface games following a user request, the apps list returned by the service
can contain games from a variety of apps platforms.

The AppDiscoveryService class is intended to be used by consumers to fetch apps:

```
auto* app_discovery_service = AppDiscoveryServiceFactory::GetForProfile(profile);
app_discovery_service->GetApps(ResultType, ResultCallback);

```

## AppFetcher

AppFetcher is an interface to be implemented by each app list provider. When a
new AppFetcher is added, a corresponding enum value should be added to
ResultType. The AppFetcherManager distinguishes between AppFetchers with this
enum value.

## AppFetcherManager
The AppFetcherManager acts as the backend of the app discovery framework and is
responsible for managing requests to AppFetchers.

---

Updated on 2021-08-26.
