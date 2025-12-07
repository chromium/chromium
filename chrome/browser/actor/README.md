# Key classes

Below is a diagram that shows lifetime and ownership relations between some
of the key classes in the actor component and the `glic` UI that they interact
with.

The diagram is not exhaustive.

```
┌──────────────────────────┐ calls ┌────────────────────────────┐                                                                                                                   
│glic:GlicPageHandler      ┼───────►glic::GlicInstanceImpl      │                                                                                                                   
│1 per GLIC WebUI          │       │N per Profile               │                                                                                                                   
└──┬───────────────────────┘       └──┬─────────────────────────┘                                                                                                                   
   │owns                              │owns                                                                                                                                         
   │                                  │                                                                                                                                             
┌──▼───────────────────────┐       ┌──▼─────────────────────────┐calls  ┌───────────────────┐                                                                                       
│glic::GlicWebClientHandler│       │glic::GlicActorTaskManager  ┼───────►ActorKeyedService  │                                                                                       
│                          ◄─┐     │N per Profile               │     ┌─┐1 per Profile      │                                                                                       
└──┬───────────────────────┘ │calls└────────────────────────────┘     │ └┬┬─────────────────┘                              calls to delegate browser actions                        
   │owns and calls           └────────────────────────────────────────┼──┘│creates and                                    ┌──────────────────────────────────────┐                  
   │                                                                  │   │owns N_task                                    │                                      │                  
┌──▼───────────────────────┐                                          │ ┌─▼─────────────────┐owns 1  ┌────────────────────▼──┐owns 1┌───────────────────┐owns 1┌─┼─────────────────┐
│mojo::Remote<WebClient>   │                                          │ │ActorTask          ┼────────►ExecutionEngine,       ┼──────►ToolController     ┼──────►Tool               │
│Displays GLIC WebUI       │                                          │ │N_task per Profile │        │implements ToolDelegate│      │N_task per Profile │      │N_task per Profile │
└──────────────────────────┘                                          │ └─┬─────────────────┘        │                       │      └───────────────────┘      └─▲─────────────────┘
                                                                      │   │                          │N_task per Profile     ┼────┐                              │                  
                                                                      │   │owns 1                    └─┬──────────────────┬──┘    │owns N_request                │                  
                                                                      │   │                            │                  │       │                              │                  
                                                                      │ ┌─▼─────────────────┐ owns 1   │                  │       │ ┌───────────────────┐ creates│                  
                                                                      │ │UiEventDispatcher  ◄──────────┘                  │       └─►ToolRequest        ┼────────┘                  
                                                                      │ │2N_task per Profile│                             │         │N_task*N_request   │                           
                                                                      │ └─┬─────────────────┘                             │         │per Profile        │                           
                                                                      │   │calls                                          │         └───────────────────┘                           
                                                                owns 1│   │                                               │                                                         
                                                                      │ ┌─▼─────────────────┐                             │                                                         
                                                                      └─►ActorUiStateManager│                             │calls                                                    
                                                                        │1 per Profile      │                             │                                                         
                                                                        └─┬─────────────────┘                             │                                                         
                                                                          │calls                                          │                                                         
                                                                          │                                               │                                                         
                                                                          │                                               │                                                         
                                                                          │                                               │                                                         
                                                                        ┌─▼──────────────────┐       ┌────────────────────▼──┐                                                      
                                                                        │ActorUiTabController│       │General Chrome code    │                                                      
                                                                        │1 per Tab           │       │                       │                                                      
                                                                        └────────────────────┘       └───────────────────────┘                                                      
```

To edit the diagram, copy it into asciiflow.com.