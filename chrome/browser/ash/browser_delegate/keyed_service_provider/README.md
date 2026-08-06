# KeyedService Providers for ChromeOS (Ash)

This directory implements "Provider" classes that bridge Chrome's `KeyedService` instances with OS-level components in ChromeOS (Ash).

## Important: Design Alternatives (Read First)

Using the **Provider pattern** should be considered a **last resort**. Before adding a new provider, evaluate whether one of the following preferred design patterns can be applied:

1. **Depend on OS Components Directly**

   If the required data or behavior is already available elsewhere in the OS, prefer depending on that component directly instead of reaching into a Chrome `KeyedService`.

2. **Inject via Constructor**

   If the client class is scoped to a specific `User` (or `Profile`), pass the service dependency directly through its constructor and retain a pointer or reference in a member variable.

3. **Remove `KeyedService` Abstraction**

   If the service is used exclusively on ChromeOS, consider removing `KeyedService` entirely. Instead, initialize the service directly during user session startup.

## When to Use a Provider

A dedicated **Provider** class (as described below) is appropriate **only when none of the alternatives apply**—for example:

* The OS component must handle multiple concurrent user sessions within a single process.

* The component relies on a `KeyedService` shared across multiple platforms for maintainability.

## Provider Architecture & Implementation

A Provider consists of three main components:

### 1. Provider Interface

* **Location:** Must reside under `//chromeos/ash/` (not under `//chrome/browser/**/ash`).

* **Structure:**

```
class FooProvider {
 public:
  virtual ~FooProvider() = default;

  // Returns the global instance of the provider.
  static FooProvider& Get();

  // Returns the Foo KeyedService for the User with the given `account_id`.
  // Returns nullptr if the service is unavailable or cannot be created.
  virtual FooKeyedService* Find(const AccountId& account_id) = 0;
};

```

### 2. Provider Implementation

* **Location:** Resides in this directory.

* **Behavior:** Implements the Provider interface. `Find()` acts as a thin wrapper around the corresponding `KeyedServiceFactory` API, converting the `AccountId` to a `BrowserContext` / `Profile`.

```
FooKeyedService* FooProviderImpl::Find(const AccountId& account_id) {
  return FooKeyedServiceFactory::GetForBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id));
}

```

### 3. Lifecycle Management

* The Provider instance is instantiated and managed by `ChromeBrowserMainPartsAsh`.

* **Lifetime:** `FooProviderImpl` outlives individual `Profile` instances.
