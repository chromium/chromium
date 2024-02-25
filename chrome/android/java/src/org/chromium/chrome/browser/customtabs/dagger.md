# Dagger in Custom Tabs

The Custom Tabs code (code in org.chromium.chrome.browser.customtabs, ...browserservices and ...webapps) makes use of Dagger for dependency injection.
It was introduced with the hope that it would be more widely adopted in Chrome, but due to other ongoing refactorings it was decided not to spread it further until a cohesive end state could be understood.

This document isn’t about the pros and cons of Dagger or an argument about whether it should be pushed further or ripped out, it is meant as a quick way for developers to understand what’s going on in Custom Tabs land and how to make changes.

## What is Dagger?

Dagger is a dependency injection framework.
You can essentially think of it as a magic box that creates your classes and wires them together for you.
For example:

```java
@ActivityScope
public class Programmer {
  private final Coffee mCoffee;

  @Inject
  public Programmer(Coffee coffee) {
    mCoffee = coffee;
  }
}
```

If you ask for a `Programmer`, `Dagger` will do its best to find a `Coffee` and then create a `Programmer` for you.

There are two annotations in the above example, `@ActivityScope` and `@Inject`.
`@Inject` is the simpler one and it tells Dagger that this is the constructor it should use (Dagger won’t create objects without an `@Inject` constructor).
`@ActivityScope` tells Dagger about the life cycle of the object - there should be one instance of this object per Android Activity.
This is probably the one you’ll be using most of the time, but there is also `@Singleton` which can be used for singletons and a couple of others.

### Modules and Components

The above is all very well and good, but how does non-Dagger code interact with the magic box that is Dagger?
We need to introduce two more concepts, *Modules* and *Components*.
(In the “I want to…” section below I’ve linked to the Modules and Components used in Chrome if you want to see a less contrived example.)

A *Module* is a way of giving objects to Dagger.
Eg:

```java
@Module
public class MyDaggerModule {
  private final Coffee mCoffee;

  public MyDaggerModule(Coffee coffee) {
    mCoffee = coffee;
  }

  @Provides
  public Coffee provideCoffee() {
    return mCoffee;
  }
}
```

In non-Dagger code you would create a `MyDaggerModule` and provide it with a `Coffee` (that you created yourself).
Armed with this, Dagger can now go and create the `Programmer`.

On the other side we have *Components*, which are how you get objects out of `Dagger`.

```java
@ActivityScope
public interface MyDaggerComponent {
  Programmer resolveProgrammer();
}
```

You write this interface, and then during the build process, Dagger will go and implement all of these methods for you.
When you call `resolveProgrammer`, Dagger will create a `Programmer` if it doesn’t already have one and give it to you.
That’s the basics of Dagger, let’s look at a few changes you may want to make and how to go about them.

## I want to...

### access a Dagger class from a Dagger class

This is pretty simple, say you want to access `Foo` from `Bar` and both of them are constructed by Dagger.
You add `Foo` to `Bar`’s constructor and Dagger should sort it all out for you.

Dagger will complain if the life cycles don’t match up (for example, you’re trying to access a `@ActivityScope` class from a `@Singleton`), and in that case you’ll have to rethink how to do things.

If there’s a circular dependency, take `Lazy<Foo>` instead of `Foo`.
A `Lazy<T>` will be resolved the first time it is accessed.

`Lazy` can also be used when you want Dagger to provide a class that won't be ready at the time it
needs to be injected. For example,`ChromeActivityCommonsModule` contains a bunch of
`Supplier<Foo>`s that will only return non-null once some stage of startup has been completed.
To use `Foo` in a class that's created earlier than it, take a `Lazy<Foo>` in the constructor.

### access a non-Dagger class from a Dagger class

There are quite a few non-Dagger classes already provided to Dagger, so see if the class you want is provided in any of the Modules:

* [BaseCustomTabActivityModule][1] - used for anything available to BaseCustomTabActivity.
* [ChromeActivityCommonsModule][2] - used for anything available to ChromeActivity.
* [ChromeAppModule][3] - used for singletons.

If not, add it to the appropriate Module and then take it in the constructor of the class you want to access it from.

### access a Dagger class from a non-Dagger class

If the class is a singleton, add a suitable *resolve* method to [ChromeAppComponent][4], you can then call `ChromeApplicationImpl.getComponent().resolveMyClass()` to get access.

If the class is `@ActivityScope`, add a suitable resolve method to [BaseCustomTabActivityComponent][5], and then get the object out of Dagger in `BaseCustomTabActivity#createComponent`.
You can then fetch the instance off `BaseCustomTabActivity`.

### override a class in unit tests

You shouldn't need to interact with Dagger in unit tests, since you'll be constructing the class yourself.
Since Dagger encourages injecting a class' dependencies in the constructor, it should make it easier to create an instance for testing.
Most likely you'll want to use fakes or mocks for the class' dependencies.

### override a class in instrumentation tests

You can use a [ModuleOverridesRule][6] which will allow you to override the instances of class passed in to Dagger.
For example, see the [RunningInChromeTest][7].

## Finals words

Hopefully that should be enough to help you make changes to Custom Tabs code.
If you need to do something more complicated, feel free to reach out to peconn@, either by email or by adding me on your code review.

[1]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/customtabs/dependency_injection/BaseCustomTabActivityModule.java
[2]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/dependency_injection/ChromeActivityCommonsModule.java
[3]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/dependency_injection/ChromeAppModule.java
[4]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/dependency_injection/ChromeAppComponent.java
[5]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/customtabs/dependency_injection/BaseCustomTabActivityComponent.java
[6]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/dependency_injection/ModuleOverridesRule.java
[7]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/browserservices/RunningInChromeTest.java
