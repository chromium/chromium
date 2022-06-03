# Digital Goods

Websites viewed in a Trusted Web Activity can use the Digital Goods API to
communicate with their corresponding APK to query information about products
that the website wants to sell to the user.
This is required for Digital Goods because according to Play Store policy,
all Digital Goods sales on Android must use Play Billing, so the Play Billing
Android APIs are the source of truth for information such as price.

## Code

* `DigitalGoodsImpl` implements the `DigitalGoods` mojo API, which handles
  requests from JavaScript. It is created by the `DigitalGoodsFactoryImpl`.
* `DigitalGoodsFactoryImpl` implements the `DigitalGoodsFactory` mojo API, which
  handles requests for new `DigitalGoods` instances. It is created by the
  `DigitalGoodsFactoryFactory`. This extra indirection allows the
  `DigitalGoodsFactory` to report success/failure when creating a `DigitalGoods`
   instance, which would not be possible if instantiating it directly.
* `TrustedWebActivityClient` is the class that talks to Trusted Web Activities.
* `DigitalGoodsAdapter` sits between `DigitalGoodsImpl` and
  `TrustedWebActivityClient`, transforming between appropriate data types.
* `DigitalGoodsConverter`, `GetDetailsConverter` and `AcknowledgeConverter`
   contain the lower level transformations that `DigitalGoodsAdapter` uses.

## Interface versions

The bundles passed from the TWA shell to Chromium may contain a version code
(see `DigitalGoodsConverter#KEY_VERSION`). If the version code is missing, it
is assumed to be `0`.

Naturally, version `0` was the first version of the interface, but it was
never used by stable Chromium or Android Browser Helper.

Version changes:
- Version `1` changed the format of response code. Previously the response code
  had been given in the Play Billing format. With version `1` Chromium expects
  the TWA client to convert the response code to a Chromium format.
