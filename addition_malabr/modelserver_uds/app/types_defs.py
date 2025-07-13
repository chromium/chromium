from typing import Literal, TypedDict

class Payload(TypedDict):
    label: Literal[
        'LABEL_READ_DATA',  
        'LABEL_SEND_DATA', 
        'LABEL_LOAD_MODEL_BERT', 
        'LABEL_INFER_MODEL_BERT'
    ]
    method: Literal['SEND', 'GET', 'POST']
    payload: str
