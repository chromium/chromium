# LCP Critical Path Predictor

This directory implements the browser side of the [Largest Contentful Paint
(LCP) Critical Path
Predictor](https://docs.google.com/document/d/1LmzYYxgMVa8aMOKbAf2LzmQM-I_mn6D55PnratBDbN4/edit?usp=sharing).

- `LCPCriticalPathPredictorDatabase`
  - Manages SQLite database and tables.
- `LCPCriticalPathPredictorPersister`
  - Implements the service-specific persistent logic using
    `LCPCriticalPathPredictorDatabase` as a backend. Currently, there is only
    one implementation, but we may introduce another implementation that has an
    in-memory database as a backend if required.
- `LCPCriticalPathPredictorKeyedService`
  - A service that predicts LCP Critical Path. Uses
    `LCPCriticalPathPredictorPersister` internally.
- `LCPCriticalPathPredictorKeyedServiceFactory`
  - A factory to create LCPCriticalPathPredictorKeyedService instances.
    Instances are created per profile.

Implementation is on-going. See http://crbug.com/1419756 for details.
